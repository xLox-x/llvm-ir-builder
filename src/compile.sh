#!/bin/bash

# usage ./run.sh file.cpp
file_name=${1##*/}

clang++ ${file_name} `llvm-config --cxxflags --ldflags --system-libs --libs core` -fno-rtti -o out.out
./out.out

