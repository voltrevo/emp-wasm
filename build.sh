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
    -o bin/2pc
