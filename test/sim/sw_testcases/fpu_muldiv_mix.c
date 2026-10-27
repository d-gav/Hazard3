// Zfinx: FPU and multiply/divide back to back, with and without a dependency
// between them, in both orders. Zfinx only.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

div->fdiv: 00000007 3eaaaaab
fdiv->div: 3eaaaaab 00000007
mul->fmul: 0000002a 41400000
fmul->mul: 41400000 0000002a
divu->fcvt: 00000007 40e00000
fcvt->div: 00000006 00000003
fsqrt->rem: 00000003 00000001
mulh->fadd: 00000001 40000000
interleaved: 00000006 40800000 00000002 3f000000 00000018 41c00000
fflags 01

*******************************************************************************/

uint32_t out[8];

static void show(const char *name, int n) {
	tb_printf("%s:", name);
	for (int i = 0; i < n; ++i)
		tb_printf(" %08x", out[i]);
	tb_printf("\n");
}

#define ONE   0x3f800000u
#define THREE 0x40400000u
#define FOUR  0x40800000u

int main() {
	write_csr(fcsr, 0);
	uint32_t *o = out;

	// Independent, back to back
	asm volatile (
		"div    t0, %1, %2\n"    // 42 / 6 = 7
		"fdiv.s t1, %3, %4\n"    // 1 / 3
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (42), "r" (6), "r" (ONE), "r" (THREE) : "t0", "t1", "memory");
	show("div->fdiv", 2);
	asm volatile (
		"fdiv.s t1, %3, %4\n"
		"div    t0, %1, %2\n"
		"sw t1, 0(%0)\n sw t0, 4(%0)\n"
		: : "r" (o), "r" (42), "r" (6), "r" (ONE), "r" (THREE) : "t0", "t1", "memory");
	show("fdiv->div", 2);
	asm volatile (
		"mul    t0, %1, %2\n"    // 42
		"fmul.s t1, %3, %4\n"    // 3 * 4 = 12
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (7), "r" (6), "r" (THREE), "r" (FOUR) : "t0", "t1", "memory");
	show("mul->fmul", 2);
	asm volatile (
		"fmul.s t1, %3, %4\n"
		"mul    t0, %1, %2\n"
		"sw t1, 0(%0)\n sw t0, 4(%0)\n"
		: : "r" (o), "r" (7), "r" (6), "r" (THREE), "r" (FOUR) : "t0", "t1", "memory");
	show("fmul->mul", 2);

	// Dependent
	asm volatile (
		"divu     t0, %1, %2\n"  // 7
		"fcvt.s.w t1, t0\n"      // 7.0
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (42), "r" (6) : "t0", "t1", "memory");
	show("divu->fcvt", 2);
	asm volatile (
		"fcvt.w.s t0, %1\n"      // 6
		"div      t1, t0, %2\n"  // 3
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (0x40c00000u), "r" (2) : "t0", "t1", "memory");
	show("fcvt->div", 2);
	asm volatile (
		"fsqrt.s  t0, %1\n"      // 3.0
		"fcvt.w.s t0, t0\n"      // 3
		"rem      t1, t0, %2\n"  // 1
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (0x41100000u /* 9.0 */), "r" (2) : "t0", "t1", "memory");
	show("fsqrt->rem", 2);
	asm volatile (
		"mulhu  t0, %1, %1\n"    // (2^16)^2 >> 32 = 1
		"fcvt.s.w t1, t0\n"
		"fadd.s t1, t1, t1\n"    // 2.0
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (0x10000) : "t0", "t1", "memory");
	show("mulh->fadd", 2);

	// Alternating chain
	asm volatile (
		"mul      t0, %1, %2\n"  // 6
		"fcvt.s.w t1, t0\n"      // 6.0
		"fsub.s   t1, t1, %3\n"  // 4.0 (6 - 2)
		"fcvt.w.s t2, t1\n"      // 4
		"div      t2, t2, %2\n"  // 2
		"fcvt.s.w t3, t2\n"
		"fdiv.s   t3, %4, t3\n"  // 1 / 2
		"mul      t4, t0, t0\n"  // 36
		"addi     t4, t4, -12\n" // 24
		"fcvt.s.w t5, t4\n"      // 24.0
		"sw t0, 0(%0)\n sw t1, 4(%0)\n sw t2, 8(%0)\n sw t3, 12(%0)\n sw t4, 16(%0)\n sw t5, 20(%0)\n"
		: : "r" (o), "r" (3), "r" (2), "r" (0x40000000u), "r" (ONE)
		: "t0", "t1", "t2", "t3", "t4", "t5", "memory");
	show("interleaved", 6);

	tb_printf("fflags %02x\n", read_csr(fflags));
	return 0;
}
