#!/bin/bash

clang++ \
    -O3 \
    -std=c++17 \
    programs/2pc.cpp \
    -I src/ \
    -I $(brew --prefix mbedtls)/include \
    -L $(brew --prefix mbedtls)/lib \
    -lmbedtls \
    -lmbedcrypto \
    -lmbedx509 \
    -o build/2pc

echo "Build successful, use ./scripts/local_test.sh to run the program."
