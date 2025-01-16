#!/bin/bash

set -euo pipefail

clang++ \
    -O3 \
    -std=c++17 \
    programs/test_mpc.cpp \
    -I src/cpp/mpc \
    -I $(brew --prefix openssl)/include \
    -L $(brew --prefix openssl)/lib \
    -lcrypto \
    -lssl \
    -o build/mpc

echo "Build successful, use ./scripts/mpc_test.sh to run the program."
