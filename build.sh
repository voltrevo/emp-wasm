#!/bin/bash

clang++ \
    -std=c++17 \
    programs/2pc.cpp \
    -I src/ \
    -I $(brew --prefix openssl)/include \
    -L $(brew --prefix openssl)/lib \
    -lcrypto \
    -lssl \
    -o bin/2pc
