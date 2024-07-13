#!/usr/bin/bash

clang++ emit_ir.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o emit_ir.out
./emit_ir.out

printf "\n"

lli out.ll

echo $?
