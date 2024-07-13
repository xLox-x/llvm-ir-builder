#!/bin/bash

file_name=${1##*/}
ll_name=${file_name/.c/.ll}
png_name=${file_name/.c/.png}

opt -dot-cfg ${ll_name}
dot .main.dot -Tpng -o ${png_name}