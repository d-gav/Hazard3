// Zfinx fflags: sticky accrual, flags from ops with rd = x0, CSR accesses
// straight after an FPU op (read-modify-write ordering), frm written straight
// before a DYN op, and trapping FP instructions leave fflags and rd unchanged.
// Zfinx only.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

sticky: 01 01 09 19
back to back: 19
rd=x0: 01
exact op keeps flags: 0c
csrr after op: 08
csrrs x0 after op: 08 fflags 08
csrrsi after op: 08 fflags 09
csrrci after op: 09 fflags 01
csrrw after op: 05 fflags 02
csrw fcsr after op: fcsr 00000060
frm then dyn: 3f800001 3f800000
fsrm then dyn: 3f800000 frm 00000002
illegal dyn: trapped 1 rd 12345678 fflags 00
illegal dyn after op: trapped 1 rd 12345678 fflags 01
illegal rm=5: trapped 1 fflags 00
fflags 00

*******************************************************************************/

#define ONE   0x3f800000u
#define TWO   0x40000000u
#define THREE 0x40400000u
#define TINY  0x33800000u   // 2^-24
#define BIG   0x7f7fffffu
#define INF   0x7f800000u

volatile uint32_t trapped;

int main() {
	uint32_t r, fl, f1, f2, f3;
	write_csr(fcsr, 0);

	// Sticky: NX, then an exact op, then DZ, then NV
	asm volatile (
		"fdiv.s %0, %5, %6\n"     // 1/3: NX
		"csrr   %1, fflags\n"
		"fadd.s %0, %5, %5\n"     // exact
		"csrr   %2, fflags\n"
		"fdiv.s %0, %5, x0\n"     // 1/0: DZ
		"csrr   %3, fflags\n"
		"fsub.s %0, %7, %7\n"     // inf - inf: NV
		"csrr   %4, fflags\n"
		: "=&r" (r), "=&r" (fl), "=&r" (f1), "=&r" (f2), "=&r" (f3)
		: "r" (ONE), "r" (THREE), "r" (INF));
	tb_printf("sticky: %02x %02x %02x %02x\n", fl, f1, f2, f3);

	write_csr(fflags, 0);
	asm volatile (
		"fdiv.s %0, %2, %3\n"
		"fdiv.s %0, %2, x0\n"
		"fsub.s %0, %4, %4\n"
		"csrr   %1, fflags\n"
		: "=&r" (r), "=&r" (fl) : "r" (ONE), "r" (THREE), "r" (INF));
	tb_printf("back to back: %02x\n", fl);

	write_csr(fflags, 0);
	asm volatile (
		"fdiv.s x0, %1, %2\n"
		"csrr   %0, fflags\n"
		: "=r" (fl) : "r" (ONE), "r" (THREE));
	tb_printf("rd=x0: %02x\n", fl);

	write_csr(fflags, 0x0c);
	asm volatile (
		"fmul.s %0, %2, %3\n"     // 1 * 3: exact
		"csrr   %1, fflags\n"
		: "=&r" (r), "=r" (fl) : "r" (ONE), "r" (THREE));
	tb_printf("exact op keeps flags: %02x\n", fl);

	// CSR accesses directly behind the op that sets the flags
	write_csr(fflags, 0);
	asm volatile (
		"fdiv.s %0, %2, x0\n"
		"csrr   %1, fflags\n"
		: "=&r" (r), "=r" (fl) : "r" (ONE));
	tb_printf("csrr after op: %02x\n", fl);

	write_csr(fflags, 0);
	asm volatile (
		"fdiv.s %0, %2, x0\n"
		"csrrs  %1, fflags, x0\n"
		: "=&r" (r), "=r" (fl) : "r" (ONE));
	tb_printf("csrrs x0 after op: %02x fflags %02x\n", fl, read_csr(fflags));

	write_csr(fflags, 0);
	asm volatile (
		"fdiv.s %0, %2, x0\n"
		"csrrsi %1, fflags, 1\n"
		: "=&r" (r), "=r" (fl) : "r" (ONE));
	tb_printf("csrrsi after op: %02x fflags %02x\n", fl, read_csr(fflags));

	write_csr(fflags, 0);
	asm volatile (
		"fdiv.s %0, %2, %3\n"     // NX
		"fdiv.s %0, %2, x0\n"     // DZ
		"csrrci %1, fflags, 8\n"
		: "=&r" (r), "=r" (fl) : "r" (ONE), "r" (THREE));
	tb_printf("csrrci after op: %02x fflags %02x\n", fl, read_csr(fflags));

	write_csr(fflags, 0);
	asm volatile (
		"fmul.s %0, %2, %3\n"     // overflow: OF NX
		"csrrw  %1, fflags, %4\n" // writes UF; the older op's flags must not survive
		: "=&r" (r), "=r" (fl) : "r" (BIG), "r" (TWO), "r" (2));
	tb_printf("csrrw after op: %02x fflags %02x\n", fl, read_csr(fflags));

	asm volatile (
		"fdiv.s %0, %1, x0\n"
		"csrw   fcsr, %2\n"
		: "=&r" (r) : "r" (ONE), "r" (0x60));
	tb_printf("csrw fcsr after op: fcsr %08x\n", read_csr(fcsr));
	write_csr(fcsr, 0);

	// frm written directly before a DYN op
	uint32_t r2;
	asm volatile (
		"csrwi  frm, 3\n"         // RUP
		"fadd.s %0, %2, %3, dyn\n"
		"csrwi  frm, 1\n"         // RTZ
		"fadd.s %1, %2, %3, dyn\n"
		: "=&r" (r), "=&r" (r2) : "r" (ONE), "r" (TINY));
	tb_printf("frm then dyn: %08x %08x\n", r, r2);
	asm volatile (
		"csrwi  frm, 2\n"         // RDN
		"fsrm   %1, %3\n"         // RUP, old = RDN
		"fadd.s %0, %2, %4, dyn\n" // 1 + 2^-24 up
		"fsrm   x0, %1\n"         // back to RDN
		"fadd.s %0, %0, %5, dyn\n" // - 2^-24, down
		: "=&r" (r), "=&r" (fl) : "r" (ONE), "r" (3), "r" (TINY), "r" (0xb3800000u));
	tb_printf("fsrm then dyn: %08x frm %08x\n", r, read_csr(frm));
	write_csr(fcsr, 0);

	// Trapping FP instructions: no flags, no rd write
	write_csr(frm, 5);
	trapped = 0;
	r = 0x12345678u;
	asm volatile ("fdiv.s %0, %1, %2, dyn" : "+r" (r) : "r" (ONE), "r" (THREE));
	tb_printf("illegal dyn: trapped %u rd %08x fflags %02x\n", trapped, r, read_csr(fflags));

	write_csr(frm, 0);
	trapped = 0;
	r = 0x12345678u;
	asm volatile (
		"fdiv.s t0, %1, %2\n"     // NX, still in M when the next one traps in X
		"csrwi  frm, 6\n"
		"fdiv.s %0, %1, x0, dyn\n"  // would be DZ
		: "+r" (r) : "r" (ONE), "r" (THREE) : "t0");
	tb_printf("illegal dyn after op: trapped %u rd %08x fflags %02x\n", trapped, r, read_csr(fflags));
	write_csr(fcsr, 0);

	trapped = 0;
	asm volatile (".word 0x18005053"); // fdiv.s x0, x0, x0, rm=5
	tb_printf("illegal rm=5: trapped %u fflags %02x\n", trapped, read_csr(fflags));

	tb_printf("fflags %02x\n", read_csr(fflags));
	return 0;
}

void __attribute__((interrupt)) handle_exception() {
	if (read_csr(mcause) != 2)
		tb_exit(1);
	trapped = 1;
	write_csr(mepc, read_csr(mepc) + 4);
}
