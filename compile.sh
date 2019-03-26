#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

cd $DIR
echo $DIR
mkdir -p build
g++ simple_sort.cpp  -O2 -std=c++14 -Wall -march=native -fopenmp -DNDEBUG -o build/simple_sort
