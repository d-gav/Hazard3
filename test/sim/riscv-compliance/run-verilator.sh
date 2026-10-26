#!/bin/bash
# WIP for plan step 3.0, not run: riscof is out of Phase 0
set -e

echo "Rebuilding simulator..."
make -f Makefile.v540 -C ../tb_verilator CONFIG=pmpfull BUILD_DIR=build-tb-pmpfull
echo "Cleaning up previous run..."
cd riscof-verilator
rm -rf riscof_work
echo "Starting..."
riscof run --config config-verilator.ini --suite ../riscv-arch-test/riscv-test-suite/ --env ../riscv-arch-test/riscv-test-suite/env
