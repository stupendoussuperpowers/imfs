#!/bin/bash


mkdir -p output

cat ./tools/magic.tmpl > magic.h
python3 ./tools/wand.py 0 >> magic.h

cat ./tools/magic.ctmpl > magic.c
python3 ./tools/wand.py 1 >> magic.c

/home/lind/lind-wasm/clang+llvm-16.0.4-x86_64-linux-gnu-ubuntu-22.04/bin/clang -pthread --target=wasm32-unknown-wasi --sysroot /home/lind/lind-wasm/src/glibc/sysroot -Wl,--import-memory,--export-memory,--max-memory=1570242560,--export=signal_callback,--export=__stack_pointer,--export=__stack_low,--export=open_grate,--export=close_grate,--export=read_grate,--export=write_grate,--export-table magic.c imfs_grate.c imfs.c -g -DLIB -DDIAG -D_GNU_SOURCE -O0 -o output/oclose_grate.wasm && /home/lind/lind-wasm/tools/binaryen/bin/wasm-opt --epoch-injection --asyncify -O2 --debuginfo output/oclose_grate.wasm -o output/oclose_grate.wasm && /home/lind/lind-wasm/src/wasmtime/target/release/wasmtime compile output/oclose_grate.wasm -o output/oclose_grate.cwasm

# rm magic.c magic.h
