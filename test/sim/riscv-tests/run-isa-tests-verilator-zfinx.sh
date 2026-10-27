set -e

# As run-isa-tests-verilator.sh, on the Zfinx tb_verilator build (config_zfinx, tb_fpu.f)
make -f Makefile.v540 -C ../tb_verilator CONFIG=zfinx DOTF=tb_fpu.f BUILD_DIR=build-tb-zfinx
cd riscv-tests/isa
make XLEN=32 clean

make -j$(nproc) XLEN=32 SKIP_V=1 rv32ui rv32uc rv32um rv32ua rv32mi

for test in $(find -name "*-p-*.bin"); do echo $test; ../../../tb_verilator/tb_fpu-zfinx --bin $test --cycles 10000 --cpuret; done
