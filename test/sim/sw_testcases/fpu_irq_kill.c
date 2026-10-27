// Zfinx: timer interrupts landing on fdiv, fsqrt (iterative) and fmul
// (pipelined). The interrupted op is killed and re-executed after mret: its
// result and flags must match an interrupt-free run, and fflags at the trap
// must not include anything from the killed op. Zfinx only.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

reference pass done
interrupts taken: yes
landed on fdiv: yes
landed on fsqrt: yes
landed on fmul: yes
flags leaked from a killed op: 0
mismatches: 0

*******************************************************************************/

#define N 48
#define PASSES 6

static const uint32_t special[] = {
	0x00000000, 0x80000000, 0x00000001, 0x807fffff, 0x00800000, 0x3f800000, 0xbf800000,
	0x40400000, 0x7f7fffff, 0x7f800000, 0xff800000, 0x7fc00000, 0x7f800001, 0x3effffff,
};

static uint32_t lfsr = 0xace1u;
static uint32_t rnd() {
	lfsr ^= lfsr << 13; lfsr ^= lfsr >> 17; lfsr ^= lfsr << 5;
	return lfsr;
}

uint32_t opa[N], opb[N];
uint32_t ref[N][6], res[N][6];

volatile uint32_t irq_count, hit_fdiv, hit_fsqrt, hit_fmul, leaked;
volatile uint32_t irq_delay_seed;

extern char kill_fdiv[], kill_fsqrt[], kill_fmul[];

static void __attribute__((noinline)) kernel(uint32_t r[N][6]) {
	for (int i = 0; i < N; ++i) {
		asm volatile (
			"csrw fflags, x0\n"
			".global kill_fdiv\n"
			"kill_fdiv: fdiv.s t0, %1, %2\n"
			"csrr t3, fflags\n"
			"csrw fflags, x0\n"
			".global kill_fsqrt\n"
			"kill_fsqrt: fsqrt.s t1, %1\n"
			"csrr t4, fflags\n"
			"csrw fflags, x0\n"
			".global kill_fmul\n"
			"kill_fmul: fmul.s t2, %1, %2\n"
			"csrr t5, fflags\n"
			"sw t0, 0(%0)\n sw t1, 4(%0)\n sw t2, 8(%0)\n"
			"sw t3, 12(%0)\n sw t4, 16(%0)\n sw t5, 20(%0)\n"
			: : "r" (r[i]), "r" (opa[i]), "r" (opb[i])
			: "t0", "t1", "t2", "t3", "t4", "t5", "memory");
	}
}

static void set_next_irq() {
	irq_delay_seed = irq_delay_seed * 1103515245u + 12345u;
	mm_timer->mtimecmp = mm_timer->mtime + 40 + (irq_delay_seed >> 16) % 61;
}

void __attribute__((interrupt)) isr_machine_timer() {
	uintptr_t mepc = read_csr(mepc);
	uint32_t fl = read_csr(fflags);
	if (mepc == (uintptr_t)kill_fdiv || mepc == (uintptr_t)kill_fsqrt || mepc == (uintptr_t)kill_fmul) {
		// fflags was cleared right before the killed op
		if (fl)
			++leaked;
		if (mepc == (uintptr_t)kill_fdiv)  ++hit_fdiv;
		if (mepc == (uintptr_t)kill_fsqrt) ++hit_fsqrt;
		if (mepc == (uintptr_t)kill_fmul)  ++hit_fmul;
	}
	++irq_count;
	set_next_irq();
}

int main() {
	write_csr(fcsr, 0);
	for (int i = 0; i < N; ++i) {
		opa[i] = rnd() & 1 ? special[rnd() % (sizeof special / 4)] : rnd();
		opb[i] = rnd() & 1 ? special[rnd() % (sizeof special / 4)] : rnd();
	}
	kernel(ref);
	tb_printf("reference pass done\n");

	uint32_t mism = 0;
	mm_timer->mtimecmp = -1u;
	mm_timer->mtimecmph = 0;
	write_csr(mie, 0x80);
	set_csr(mstatus, 0x8);
	for (int p = 0; p < PASSES; ++p) {
		irq_delay_seed = p;
		set_next_irq();
		kernel(res);
		for (int i = 0; i < N; ++i)
			for (int j = 0; j < 6; ++j)
				if (res[i][j] != ref[i][j]) {
					if (mism < 8)
						tb_printf("pass %d op %d word %d: %08x, want %08x\n", p, i, j, res[i][j], ref[i][j]);
					++mism;
				}
	}
	clear_csr(mstatus, 0x8);

	tb_printf("interrupts taken: %s\n", irq_count ? "yes" : "no");
	tb_printf("landed on fdiv: %s\n", hit_fdiv ? "yes" : "no");
	tb_printf("landed on fsqrt: %s\n", hit_fsqrt ? "yes" : "no");
	tb_printf("landed on fmul: %s\n", hit_fmul ? "yes" : "no");
	tb_printf("flags leaked from a killed op: %u\n", leaked);
	tb_printf("mismatches: %u\n", mism);
	return 0;
}
