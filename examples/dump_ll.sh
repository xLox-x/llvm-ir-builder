#!/bin/bash

file_name=${1##*/}
ll_name=${file_name/.c/.ll}

clang -emit-llvm ${file_name} -S -o ${ll_name}