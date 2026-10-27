// Zfinx (single precision, no fused multiply-add) for rvcpp. Arithmetic,
// compares and conversions come from the RISC-V build of SoftFloat;
// sign injection, min/max and classify follow the spec, as in
// arcsim-soc rtl/fpu/test/ref_model.c.

#include "rv_core.h"

extern "C" {
#include "softfloat.h"
}

static const uint32_t CNAN = 0x7fc00000u;

static bool is_nan(uint32_t a) {return (a & 0x7f800000u) == 0x7f800000u && (a & 0x7fffffu);}
static bool is_snan(uint32_t a) {return is_nan(a) && !(a & 0x400000u);}

static uint32_t fclass(uint32_t a) {
	uint32_t sign = a >> 31, exp = a >> 23 & 0xff, man = a & 0x7fffff;
	if (exp == 0xff) {
		if (man == 0) return sign ? 1u << 0 : 1u << 7;
		return man >> 22 ? 1u << 9 : 1u << 8;
	}
	if (exp == 0) {
		if (man == 0) return sign ? 1u << 3 : 1u << 4;
		return sign ? 1u << 2 : 1u << 5;
	}
	return sign ? 1u << 1 : 1u << 6;
}

static uint32_t minmax(uint32_t a, uint32_t b, bool max) {
	if (is_snan(a) || is_snan(b)) softfloat_exceptionFlags |= softfloat_flag_invalid;
	if (is_nan(a) && is_nan(b)) return CNAN;
	if (is_nan(a)) return b;
	if (is_nan(b)) return a;
	if (a == b) return a;
	if ((a | b) == 0x80000000u) return max ? 0 : 0x80000000u;
	uint_fast8_t f = softfloat_exceptionFlags;
	bool lt = f32_lt(float32_t{a}, float32_t{b});
	softfloat_exceptionFlags = f;
	return (lt ^ max) ? a : b;
}

bool RVCore::fp_exec(uint32_t instr, ux_t rs1, ux_t rs2, std::optional<ux_t> &rd_wdata) {
	static const uint_fast8_t rm_sf[5] = {
		softfloat_round_near_even, softfloat_round_minMag, softfloat_round_min,
		softfloat_round_max, softfloat_round_near_maxMag
	};
	uint funct7 = instr >> 25;
	uint rm = instr >> 12 & 0x7;
	uint rs2_field = instr >> 20 & 0x1f;
	float32_t a{rs1}, b{rs2};

	// Ops that round: static rm 101/110 is reserved, DYN takes frm (illegal if > 4)
	bool rounds =
		funct7 == 0x00 || funct7 == 0x04 || funct7 == 0x08 || funct7 == 0x0c ||
		funct7 == 0x2c || funct7 == 0x60 || funct7 == 0x68;
	if (rounds) {
		if (rm == 7)
			rm = csr.get_frm();
		if (rm > 4)
			return false;
		softfloat_roundingMode = rm_sf[rm];
	}
	softfloat_detectTininess = softfloat_tininess_afterRounding;
	softfloat_exceptionFlags = 0;

	ux_t result;
	switch (funct7) {
	case 0x00: result = f32_add(a, b).v; break;
	case 0x04: result = f32_sub(a, b).v; break;
	case 0x08: result = f32_mul(a, b).v; break;
	case 0x0c: result = f32_div(a, b).v; break;
	case 0x2c:
		if (rs2_field != 0) return false;
		result = f32_sqrt(a).v;
		break;
	case 0x10:
		if      (rm == 0) result = (rs1 & 0x7fffffffu) | (rs2 & 0x80000000u);
		else if (rm == 1) result = (rs1 & 0x7fffffffu) | (~rs2 & 0x80000000u);
		else if (rm == 2) result = rs1 ^ (rs2 & 0x80000000u);
		else return false;
		break;
	case 0x14:
		if (rm > 1) return false;
		result = minmax(rs1, rs2, rm == 1);
		break;
	case 0x50:
		if      (rm == 2) result = f32_eq(a, b);
		else if (rm == 1) result = f32_lt(a, b);
		else if (rm == 0) result = f32_le(a, b);
		else return false;
		break;
	case 0x60:
		if      (rs2_field == 0) result = (ux_t)f32_to_i32(a, softfloat_roundingMode, true);
		else if (rs2_field == 1) result = f32_to_ui32(a, softfloat_roundingMode, true);
		else return false;
		break;
	case 0x68:
		if      (rs2_field == 0) result = i32_to_f32((int32_t)rs1).v;
		else if (rs2_field == 1) result = ui32_to_f32(rs1).v;
		else return false;
		break;
	case 0x70:
		if (rs2_field != 0 || rm != 1) return false;
		result = fclass(rs1);
		break;
	default:
		return false;
	}
	csr.fp_accrue(softfloat_exceptionFlags);
	rd_wdata = result;
	return true;
}
