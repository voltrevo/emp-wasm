#!/bin/bash

clang++ \
    -O3 \
    -std=c++17 \
    programs/test_mpc.cpp \
    -I src/cpp/mpc \
    -I $(brew --prefix mbedtls)/include \
    -L $(brew --prefix mbedtls)/lib \
    -lmbedtls \
    -lmbedcrypto \
    -lmbedx509 \
    -o build/mpc

echo "Build successful, use ./scripts/local_test.sh to run the program."
