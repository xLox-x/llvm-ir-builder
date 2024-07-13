#!/bin/bash

clang++ module.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o toy.out
./toy.out
