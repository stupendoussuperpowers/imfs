#!/bin/bash

set -x 

RUST_BACKTRACE=1 /home/lind/lind-wasm/src/wasmtime/target/release/wasmtime run --allow-precompiled --wasi threads=y --wasi preview2=n output/oclose_grate.wasm runopen.wasm 
