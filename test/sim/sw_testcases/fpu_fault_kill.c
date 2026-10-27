// Zfinx: a load or store bus fault in M kills the FPU op behind it in X. The
// killed op must leave no result and no flags. The handler either skips just
// the faulting access (the FPU op then runs normally) or both. Zfinx only.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

lw   fdiv.s    skip 1: mcause 5 fflags at trap 00 -> 3eaaaaab fflags 01
lw   fdiv.s    skip 2: mcause 5 fflags at trap 00 -> 12345678 fflags 00
lw   fsqrt.s   skip 1: mcause 5 fflags at trap 00 -> 3fb504f3 fflags 01
lw   fsqrt.s   skip 2: mcause 5 fflags at trap 00 -> 12345678 fflags 00
lw   fmul.s    skip 1: mcause 5 fflags at trap 00 -> 7f800000 fflags 05
lw   fmul.s    skip 2: mcause 5 fflags at trap 00 -> 12345678 fflags 00
lw   fcvt.w.s  skip 1: mcause 5 fflags at trap 00 -> 00000001 fflags 01
lw   fcvt.w.s  skip 2: mcause 5 fflags at trap 00 -> 12345678 fflags 00
sw   fdiv.s    skip 1: mcause 7 fflags at trap 00 -> 3eaaaaab fflags 01
sw   fdiv.s    skip 2: mcause 7 fflags at trap 00 -> 12345678 fflags 00
sw   fmul.s    skip 1: mcause 7 fflags at trap 00 -> 7f800000 fflags 05
sw   fmul.s    skip 2: mcause 7 fflags at trap 00 -> 12345678 fflags 00

*******************************************************************************/

volatile uint32_t skip, trap_cause, trap_fflags;

#define BAD 0xcdef1234u

// Faulting access, then the FPU op; both 32-bit so the handler can skip by 4 or 8
#define CASE(ACC, OPNAME, OP) \
	for (skip = 1; skip <= 2; ++skip) { \
		uint32_t r = 0x12345678u; \
		write_csr(fflags, 0); \
		asm volatile ( \
			".option push\n.option norvc\n" \
			ACC "\n" \
			OP "\n" \
			".option pop\n" \
			: "+r" (r) : "r" (BAD), "r" (a), "r" (b) : "t2", "memory"); \
		tb_printf("%-4s %-9s skip %u: mcause %u fflags at trap %02x -> %08x fflags %02x\n", \
			ACC[0] == 'l' ? "lw" : "sw", OPNAME, skip, trap_cause, trap_fflags, r, read_csr(fflags)); \
	}

int main() {
	uint32_t a, b;
	a = 0x3f800000u; b = 0x40400000u;
	CASE("lw t2, 0(%1)", "fdiv.s", "fdiv.s %0, %2, %3")
	a = 0x40000000u;
	CASE("lw t2, 0(%1)", "fsqrt.s", "fsqrt.s %0, %2")
	a = 0x7f7fffffu; b = 0x40000000u;
	CASE("lw t2, 0(%1)", "fmul.s", "fmul.s %0, %2, %3")
	a = 0x3fc00000u;
	CASE("lw t2, 0(%1)", "fcvt.w.s", "fcvt.w.s %0, %2, rtz")
	a = 0x3f800000u; b = 0x40400000u;
	CASE("sw x0, 0(%1)", "fdiv.s", "fdiv.s %0, %2, %3")
	a = 0x7f7fffffu; b = 0x40000000u;
	CASE("sw x0, 0(%1)", "fmul.s", "fmul.s %0, %2, %3")
	return 0;
}

void __attribute__((interrupt)) handle_exception() {
	trap_cause = read_csr(mcause);
	trap_fflags = read_csr(fflags);
	write_csr(mepc, read_csr(mepc) + 4 * skip);
}
