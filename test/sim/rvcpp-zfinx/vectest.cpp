// Runs rtl/fpu/test vector files (testfloat_gen format) through rvcpp-zfinx's
// OP-FP model: static rm on even vectors, DYN through frm on odd ones.
// usage: vectest <op> <rm> <file>
#include "rv_core.h"
#include "encoding/rv_csr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Op {const char *name; int nin; uint funct7, rs2f; int rm;};
static const Op ops[] = {
	{"f32_add", 2, 0x00, 0, -1}, {"f32_sub", 2, 0x04, 0, -1}, {"f32_mul", 2, 0x08, 0, -1},
	{"f32_div", 2, 0x0c, 0, -1}, {"f32_sqrt", 1, 0x2c, 0, -1},
	{"f32_eq", 2, 0x50, 0, 2}, {"f32_lt", 2, 0x50, 0, 1}, {"f32_le", 2, 0x50, 0, 0},
	{"f32_min", 2, 0x14, 0, 0}, {"f32_max", 2, 0x14, 0, 1},
	{"f32_sgnj", 2, 0x10, 0, 0}, {"f32_sgnjn", 2, 0x10, 0, 1}, {"f32_sgnjx", 2, 0x10, 0, 2},
	{"f32_class", 1, 0x70, 0, 1},
	{"f32_to_i32", 1, 0x60, 0, -1}, {"f32_to_ui32", 1, 0x60, 1, -1},
	{"i32_to_f32", 1, 0x68, 0, -1}, {"ui32_to_f32", 1, 0x68, 1, -1},
};

int main(int argc, char **argv) {
	if (argc < 4) {fprintf(stderr, "usage: %s op rm file\n", argv[0]); return 2;}
	const char *name = !strncmp(argv[1], "rv_", 3) ? argv[1] + 3 : argv[1];
	const Op *op = nullptr;
	for (auto &o : ops) if (!strcmp(o.name, name)) op = &o;
	if (!op) {fprintf(stderr, "unknown op %s\n", argv[1]); return 2;}
	const char *rms[] = {"rne", "rtz", "rdn", "rup", "rmm"};
	int rm = -1;
	for (int i = 0; i < 5; ++i) if (!strcmp(rms[i], argv[2])) rm = i;
	if (rm < 0) {fprintf(stderr, "unknown rm %s\n", argv[2]); return 2;}
	FILE *f = fopen(argv[3], "r");
	if (!f) {perror(argv[3]); return 2;}
	MemMap32 mem;
	RVCore core(mem, 0, 0, 0);
	core.csr.write(CSR_FRM, rm);
	core.csr.step(1);
	char line[256];
	unsigned long n = 0, bad = 0;
	while (fgets(line, sizeof line, f)) {
		uint32_t in[2] = {0, 0};
		char *p = line;
		for (int i = 0; i < op->nin; ++i) in[i] = strtoul(p, &p, 16);
		uint32_t res = strtoul(p, &p, 16);
		uint32_t flags = strtoul(p, &p, 16);
		uint frm = op->rm >= 0 ? op->rm : (n & 1) ? 7 : rm;
		uint32_t instr = op->funct7 << 25 | (op->nin == 2 ? 2u : op->rs2f) << 20 | 1u << 15 | frm << 12 | 3u << 7 | 0x53;
		core.csr.write(CSR_FFLAGS, 0);
		core.csr.step(1);
		std::optional<ux_t> rd;
		bool ok = core.fp_exec(instr, in[0], in[1], rd);
		uint32_t got_flags = *core.csr.read(CSR_FFLAGS);
		if (!ok || !rd || *rd != res || got_flags != flags) {
			if (bad++ < 10)
				printf("  %08x %08x: want %08x %02x, got %s %08x %02x\n", in[0], in[1], res, flags,
					ok ? "" : "ILLEGAL", rd ? *rd : 0, got_flags);
		}
		++n;
	}
	printf("%s %-16s %s %lu vectors, %lu mismatches\n", bad ? "FAIL" : "PASS", argv[1], argv[2], n, bad);
	return bad != 0;
}
