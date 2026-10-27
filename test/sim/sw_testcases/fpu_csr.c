// Zfinx fcsr, frm and fflags: reset value, field masks, aliasing, set/clear,
// immediate forms, misa.F/D clear. Zfinx only.
#include "tb_cxxrtl_io.h"
#include "hazard3_csr.h"
#include <stdint.h>

/*EXPECTED-OUTPUT***************************************************************

misa F D: 0 0
reset fcsr 00000000
fcsr <- ffffffff: fcsr 000000ff frm 00000007 fflags 0000001f
fflags <- 0: fcsr 000000e0
frm <- 2: fcsr 00000040
frm <- ffffffff: fcsr 000000e0
fflags <- 0000ffff: fcsr 000000ff
fcsr <- 0: fcsr 00000000
fflags set 15: fcsr 00000015
fflags clear 05: fcsr 00000010
frm set 3: fcsr 00000070
csrrw fcsr 123: old 00000070 fcsr 00000023
csrrwi fflags 1f: old 00000003 fcsr 0000003f
csrrsi frm 4: old 00000001 fcsr 000000bf
csrrci fcsr 1f: old 000000bf fcsr 000000a0
csrrs fflags x0: 00000000 fcsr 000000a0

*******************************************************************************/

#define SHOW(msg) tb_printf("%s: fcsr %08x\n", msg, read_csr(fcsr))

int main() {
	uint32_t misa = read_csr(misa);
	tb_printf("misa F D: %u %u\n", (misa >> 5) & 1, (misa >> 3) & 1);
	tb_printf("reset fcsr %08x\n", read_csr(fcsr));
	write_csr(fcsr, 0xffffffffu);
	tb_printf("fcsr <- ffffffff: fcsr %08x frm %08x fflags %08x\n", read_csr(fcsr), read_csr(frm), read_csr(fflags));
	write_csr(fflags, 0);
	SHOW("fflags <- 0");
	write_csr(frm, 2);
	SHOW("frm <- 2");
	write_csr(frm, 0xffffffffu);
	SHOW("frm <- ffffffff");
	write_csr(fflags, 0xffffu);
	SHOW("fflags <- 0000ffff");
	write_csr(fcsr, 0);
	SHOW("fcsr <- 0");
	set_csr(fflags, 0x15);
	SHOW("fflags set 15");
	clear_csr(fflags, 0x05);
	SHOW("fflags clear 05");
	set_csr(frm, 3);
	SHOW("frm set 3");
	uint32_t old;
	old = read_write_csr(fcsr, 0x123);
	tb_printf("csrrw fcsr 123: old %08x fcsr %08x\n", old, read_csr(fcsr));
	asm volatile ("csrrwi %0, fflags, 0x1f" : "=r" (old));
	tb_printf("csrrwi fflags 1f: old %08x fcsr %08x\n", old, read_csr(fcsr));
	asm volatile ("csrrsi %0, frm, 4" : "=r" (old));
	tb_printf("csrrsi frm 4: old %08x fcsr %08x\n", old, read_csr(fcsr));
	asm volatile ("csrrci %0, fcsr, 0x1f" : "=r" (old));
	tb_printf("csrrci fcsr 1f: old %08x fcsr %08x\n", old, read_csr(fcsr));
	asm volatile ("csrrs %0, fflags, x0" : "=r" (old));
	tb_printf("csrrs fflags x0: %08x fcsr %08x\n", old, read_csr(fcsr));
	return 0;
}
