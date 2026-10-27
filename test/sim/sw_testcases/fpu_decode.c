// Zfinx decode: which OP-FP (and other FP) encodings trap as illegal. Zfinx only.
// All encodings use x0 for rd/rs1/rs2. frm = 0, so DYN (rm = 7) is legal.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

fadd.s rm=0: ok
fadd.s rm=1: ok
fadd.s rm=2: ok
fadd.s rm=3: ok
fadd.s rm=4: ok
fadd.s rm=5: illegal
fadd.s rm=6: illegal
fadd.s rm=7: ok
fsub.s rm=0: ok
fsub.s rm=5: illegal
fmul.s rm=0: ok
fmul.s rm=6: illegal
fdiv.s rm=4: ok
fdiv.s rm=5: illegal
fsqrt.s: ok
fsqrt.s rs2=1: illegal
fsqrt.s rm=6: illegal
fsgnj.s: ok
fsgnjn.s: ok
fsgnjx.s: ok
fsgnj f3=3: illegal
fmin.s: ok
fmax.s: ok
fminmax f3=2: illegal
fcvt.w.s: ok
fcvt.wu.s: ok
fcvt.l.s: illegal
fcvt.w.s rm=5: illegal
fcvt.s.w: ok
fcvt.s.wu: ok
fcvt.s.lu: illegal
fcvt.s.w rm=6: illegal
feq.s: ok
flt.s: ok
fle.s: ok
fcmp f3=3: illegal
fclass.s: ok
fclass.s rs2=1: illegal
fmv.x.w: illegal
fmv.w.x: illegal
fadd.d: illegal
fadd.h: illegal
fadd.q: illegal
fcvt.s.d: illegal
fcvt.d.s: illegal
fsqrt.d: illegal
fmadd.s: illegal
fmsub.s: illegal
fnmsub.s: illegal
fnmadd.s: illegal
flw: illegal
fsw: illegal

*******************************************************************************/

volatile uint32_t trapped;

static void report(const char *name) {
	tb_printf("%s: %s\n", name, trapped ? "illegal" : "ok");
	trapped = 0;
}

int main() {
	write_csr(fcsr, 0);
	trapped = 0;
	asm volatile (".word 0x00000053"); report("fadd.s rm=0");
	asm volatile (".word 0x00001053"); report("fadd.s rm=1");
	asm volatile (".word 0x00002053"); report("fadd.s rm=2");
	asm volatile (".word 0x00003053"); report("fadd.s rm=3");
	asm volatile (".word 0x00004053"); report("fadd.s rm=4");
	asm volatile (".word 0x00005053"); report("fadd.s rm=5");
	asm volatile (".word 0x00006053"); report("fadd.s rm=6");
	asm volatile (".word 0x00007053"); report("fadd.s rm=7");
	asm volatile (".word 0x08000053"); report("fsub.s rm=0");
	asm volatile (".word 0x08005053"); report("fsub.s rm=5");
	asm volatile (".word 0x10000053"); report("fmul.s rm=0");
	asm volatile (".word 0x10006053"); report("fmul.s rm=6");
	asm volatile (".word 0x18004053"); report("fdiv.s rm=4");
	asm volatile (".word 0x18005053"); report("fdiv.s rm=5");
	asm volatile (".word 0x58000053"); report("fsqrt.s");
	asm volatile (".word 0x58100053"); report("fsqrt.s rs2=1");
	asm volatile (".word 0x58006053"); report("fsqrt.s rm=6");
	asm volatile (".word 0x20000053"); report("fsgnj.s");
	asm volatile (".word 0x20001053"); report("fsgnjn.s");
	asm volatile (".word 0x20002053"); report("fsgnjx.s");
	asm volatile (".word 0x20003053"); report("fsgnj f3=3");
	asm volatile (".word 0x28000053"); report("fmin.s");
	asm volatile (".word 0x28001053"); report("fmax.s");
	asm volatile (".word 0x28002053"); report("fminmax f3=2");
	asm volatile (".word 0xc0001053"); report("fcvt.w.s");
	asm volatile (".word 0xc0101053"); report("fcvt.wu.s");
	asm volatile (".word 0xc0201053"); report("fcvt.l.s");
	asm volatile (".word 0xc0005053"); report("fcvt.w.s rm=5");
	asm volatile (".word 0xd0007053"); report("fcvt.s.w");
	asm volatile (".word 0xd0100053"); report("fcvt.s.wu");
	asm volatile (".word 0xd0300053"); report("fcvt.s.lu");
	asm volatile (".word 0xd0006053"); report("fcvt.s.w rm=6");
	asm volatile (".word 0xa0002053"); report("feq.s");
	asm volatile (".word 0xa0001053"); report("flt.s");
	asm volatile (".word 0xa0000053"); report("fle.s");
	asm volatile (".word 0xa0003053"); report("fcmp f3=3");
	asm volatile (".word 0xe0001053"); report("fclass.s");
	asm volatile (".word 0xe0101053"); report("fclass.s rs2=1");
	asm volatile (".word 0xe0000053"); report("fmv.x.w");
	asm volatile (".word 0xf0000053"); report("fmv.w.x");
	asm volatile (".word 0x02000053"); report("fadd.d");
	asm volatile (".word 0x04000053"); report("fadd.h");
	asm volatile (".word 0x06000053"); report("fadd.q");
	asm volatile (".word 0x40100053"); report("fcvt.s.d");
	asm volatile (".word 0x42000053"); report("fcvt.d.s");
	asm volatile (".word 0x5a000053"); report("fsqrt.d");
	asm volatile (".word 0x00000043"); report("fmadd.s");
	asm volatile (".word 0x00000047"); report("fmsub.s");
	asm volatile (".word 0x0000004b"); report("fnmsub.s");
	asm volatile (".word 0x0000004f"); report("fnmadd.s");
	asm volatile (".word 0x00002007"); report("flw");
	asm volatile (".word 0x00002027"); report("fsw");
	return 0;
}

void __attribute__((interrupt)) handle_exception() {
	if (read_csr(mcause) != 2)
		tb_exit(1);
	trapped = 1;
	write_csr(mepc, read_csr(mepc) + 4);
}
