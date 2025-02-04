#!/bin/bash

set -euo pipefail

# Note: -D__debug does not seem to impact performance, and is good for testing
clang++ \
    -O3 \
    -std=c++17 \
    -D__debug \
    programs/test_mpc.cpp \
    -I src/cpp \
    -I $(brew --prefix mbedtls)/include \
    -L $(brew --prefix mbedtls)/lib \
    -lmbedtls \
    -lmbedcrypto \
    -lmbedx509 \
    -o build/mpc

echo "Build successful, use ./scripts/mpc_test.sh to run the program."
