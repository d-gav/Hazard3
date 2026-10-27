// Zfinx dependent chains through the bypass: FPU->FPU, FPU->ALU (incl.
// branch, mul, CSR), ALU->FPU, load->FPU, FPU->store (data and address),
// at distance 1, 2 and 3. Zfinx only.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

fpu->fpu: 40800000 41800000 41400000 40400000 3fddb3d7
fpu->fpu rs1=rs2: 40800000 40800000 41800000
fpu->alu: 81000000 40800000 00000005 00000006 00000019
fpu->branch: 00000001 00000000
alu->fpu: 40400000 40e00000 3f800000 40400000
load->fpu: 40c00000 40000000 40400000 00000004
fpu->store: 40800000 40800000 0000002a
distance 2: 40800000 41000000 41000000 00000008
distance 3: 40800000 41000000 41000000 00000008
fpu->csr: 40800000 40800000
fflags 01

*******************************************************************************/

volatile uint32_t mem[8] __attribute__((aligned(16)));
uint32_t out[8];

static void show(const char *name, int n) {
	tb_printf("%s:", name);
	for (int i = 0; i < n; ++i)
		tb_printf(" %08x", out[i]);
	tb_printf("\n");
}

#define ONE   0x3f800000u
#define TWO   0x40000000u
#define THREE 0x40400000u

int main() {
	write_csr(fcsr, 0);
	uint32_t *o = out;

	// FPU -> FPU: each op uses the previous result
	asm volatile (
		"fadd.s  t0, %1, %2\n"   // 1 + 3 = 4
		"fmul.s  t1, t0, t0\n"   // 16
		"fsub.s  t2, t1, t0\n"   // 12
		"fdiv.s  t3, t2, t0\n"   // 3
		"fsqrt.s t4, t3\n"       // sqrt(3), inexact
		"sw t0, 0(%0)\n sw t1, 4(%0)\n sw t2, 8(%0)\n sw t3, 12(%0)\n sw t4, 16(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE) : "t0", "t1", "t2", "t3", "t4", "memory");
	show("fpu->fpu", 5);

	// Result overwrites its own source, then both operands bypassed
	asm volatile (
		"mv t0, %1\n"
		"fadd.s  t0, t0, %2\n"   // 4
		"sw t0, 0(%0)\n"
		"fmax.s  t1, t0, t0\n"   // 4
		"fmul.s  t2, t1, t0\n"   // 16
		"sw t1, 4(%0)\n sw t2, 8(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE) : "t0", "t1", "t2", "memory");
	show("fpu->fpu rs1=rs2", 3);

	// FPU -> ALU and multiply
	asm volatile (
		"fadd.s  t0, %1, %2\n"   // 4.0 = 40800000
		"add     t1, t0, t0\n"   // 81000000
		"fsgnj.s t2, t0, t0\n"
		"mv      t3, t2\n"
		"fcvt.w.s t4, %3, rtz\n" // 5
		"addi    t5, t4, 1\n"    // 6
		"fcvt.w.s t6, %3, rtz\n"
		"mul     a5, t6, t6\n"   // 25
		"sw t1, 0(%0)\n sw t3, 4(%0)\n sw t4, 8(%0)\n sw t5, 12(%0)\n sw a5, 16(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE), "r" (0x40b00000u /* 5.5 */)
		: "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a5", "memory");
	show("fpu->alu", 5);

	// FPU -> branch: compare result decides a branch straight away
	asm volatile (
		"li t1, 0\n"
		"flt.s t0, %1, %2\n"
		"beqz t0, 1f\n"
		"li t1, 1\n"
		"1:\n"
		"sw t1, 0(%0)\n"
		"li t1, 0\n"
		"flt.s t0, %2, %1\n"
		"beqz t0, 2f\n"
		"li t1, 1\n"
		"2:\n"
		"sw t1, 4(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE) : "t0", "t1", "memory");
	show("fpu->branch", 2);

	// ALU -> FPU
	asm volatile (
		"lui  t0, 0x40400\n"     // 3.0
		"fadd.s t1, t0, x0\n"    // 3 + 0
		"addi t2, x0, 7\n"
		"fcvt.s.w t3, t2\n"      // 7.0
		"lui  t4, 0x3f800\n"
		"fmin.s t5, t4, t0\n"    // 1.0
		"fmax.s t6, t4, t0\n"    // 3.0
		"sw t1, 0(%0)\n sw t3, 4(%0)\n sw t5, 8(%0)\n sw t6, 12(%0)\n"
		: : "r" (o) : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
	show("alu->fpu", 4);

	// load -> FPU (load-use)
	mem[0] = THREE;
	mem[1] = 0x40800000u;        // 4.0
	asm volatile (
		"lw t0, 0(%1)\n"
		"fadd.s t1, t0, t0\n"    // 6
		"lw t2, 4(%1)\n"
		"fsqrt.s t3, t2\n"       // 2
		"lw t4, 0(%1)\n"
		"lw t5, 4(%1)\n"
		"fmin.s t6, t5, t4\n"    // 3
		"lw a5, 4(%1)\n"
		"fcvt.w.s a4, a5\n"      // 4
		"sw t1, 0(%0)\n sw t3, 4(%0)\n sw t6, 8(%0)\n sw a4, 12(%0)\n"
		: : "r" (o), "r" (mem) : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "a4", "a5", "memory");
	show("load->fpu", 4);

	// FPU -> store data, FPU -> store/load address
	float fa = (float)(uint32_t)(uintptr_t)&mem[4];
	uint32_t addr_f;
	__builtin_memcpy(&addr_f, &fa, 4);
	asm volatile (
		"fadd.s t0, %2, %3\n"
		"sw t0, 8(%1)\n"         // store data straight from the FPU
		"fadd.s t1, %2, %3\n"
		"sh t1, 12(%1)\n"
		"srli t2, t1, 16\n"
		"sh t2, 14(%1)\n"
		"fcvt.wu.s t3, %4\n"     // address from the FPU
		"li t4, 42\n"
		"sw t4, 0(t3)\n"
		"lw t5, 0(t3)\n"
		"lw t0, 8(%1)\n"
		"lw t1, 12(%1)\n"
		"sw t0, 0(%0)\n sw t1, 4(%0)\n sw t5, 8(%0)\n"
		: : "r" (o), "r" (mem), "r" (ONE), "r" (THREE), "r" (addr_f)
		: "t0", "t1", "t2", "t3", "t4", "t5", "memory");
	show("fpu->store", 3);

	// Distance 2 and 3
	asm volatile (
		"fadd.s t0, %1, %2\n"
		"nop\n"
		"fadd.s t1, t0, t0\n"    // 8
		"nop\n"
		"fmax.s t2, t1, t0\n"    // 8
		"nop\n"
		"fcvt.w.s t3, t2\n"      // 8
		"sw t0, 0(%0)\n sw t1, 4(%0)\n sw t2, 8(%0)\n sw t3, 12(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE) : "t0", "t1", "t2", "t3", "memory");
	show("distance 2", 4);
	asm volatile (
		"fadd.s t0, %1, %2\n"
		"nop\n nop\n"
		"fadd.s t1, t0, t0\n"
		"nop\n nop\n"
		"fmax.s t2, t1, t0\n"
		"nop\n nop\n"
		"fcvt.w.s t3, t2\n"
		"sw t0, 0(%0)\n sw t1, 4(%0)\n sw t2, 8(%0)\n sw t3, 12(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE) : "t0", "t1", "t2", "t3", "memory");
	show("distance 3", 4);

	// FPU -> CSR write
	asm volatile (
		"fadd.s t0, %1, %2\n"
		"csrw mscratch, t0\n"
		"csrr t1, mscratch\n"
		"sw t0, 0(%0)\n sw t1, 4(%0)\n"
		: : "r" (o), "r" (ONE), "r" (THREE) : "t0", "t1", "memory");
	show("fpu->csr", 2);

	tb_printf("fflags %02x\n", read_csr(fflags));
	return 0;
}
