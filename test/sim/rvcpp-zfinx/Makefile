# rvcpp with Zfinx: rv_core.cpp, rv_csr.cpp and their headers are copies of
# ../rvcpp with OP-FP and fflags/frm/fcsr added; rv_fpu.cpp is new. main.cpp
# and the other headers come from ../rvcpp. Needs $SOFTFLOAT_RV (fpu-env.sh).
SRCS=rv_core.cpp rv_csr.cpp rv_fpu.cpp ../rvcpp/main.cpp
EXECUTABLE:=rvcpp-zfinx

.SUFFIXES:
.PHONY: all clean tb

all: $(EXECUTABLE)

$(EXECUTABLE): $(SRCS) $(wildcard include/*.h ../rvcpp/include/*.h)
	g++ -std=c++17 -O3 -Wall -Wextra -I include -I ../rvcpp/include -I $(SOFTFLOAT_RV)/include $(SRCS) $(SOFTFLOAT_RV)/lib/softfloat.a -o $(EXECUTABLE)

tb: all

clean:
	rm -f $(EXECUTABLE) vectest vectest.log

# Model check: the rtl/fpu/test vectors (no FMA) through fp_exec.
# make check-vectors VEC=<arcsim-soc>/rtl/fpu/test/vectors
VEC ?= $(HAZARD3_FPU_HOME)/test/vectors
vectest: vectest.cpp rv_core.cpp rv_csr.cpp rv_fpu.cpp $(wildcard include/*.h ../rvcpp/include/*.h)
	g++ -std=c++17 -O3 -Wall -Wextra -I include -I ../rvcpp/include -I $(SOFTFLOAT_RV)/include vectest.cpp rv_core.cpp rv_csr.cpp rv_fpu.cpp $(SOFTFLOAT_RV)/lib/softfloat.a -o vectest

.PHONY: check-vectors
check-vectors: vectest
	@ls $(VEC)/*.tf | grep -v mulAdd | while read f; do b=$${f##*/}; b=$${b%.tf}; echo "$${b%.*} $${b##*.} $$f"; done | \
	  xargs -P 32 -L1 ./vectest | sort | tee vectest.log; ! grep -q '^FAIL' vectest.log
