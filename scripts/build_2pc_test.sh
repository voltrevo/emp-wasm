#!/bin/bash

set -euo pipefail

# Note: -D__debug is noticeably slower, but is good for testing
clang++ \
    -O3 \
    -D__debug \
    -std=c++17 \
    programs/test_2pc.cpp \
    -I src/cpp/ \
    -I $(brew --prefix mbedtls)/include \
    -L $(brew --prefix mbedtls)/lib \
    -lmbedtls \
    -lmbedcrypto \
    -lmbedx509 \
    -o build/2pc

echo "Build successful, use ./scripts/2pc_test.sh to run the program."
