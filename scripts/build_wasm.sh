#!/bin/bash

set -euo pipefail

# Variables
MBEDTLS_DIR="./external/mbedtls"
BUILD_DIR="$MBEDTLS_DIR/build/library"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Please run ./scripts/build_mbedtls.sh first"
  exit 1
fi

# Emscripten build
em++ programs/jslib.cpp -sASYNCIFY -o build/index.html \
  -O3 \
  -Wall \
  -Wextra \
  -pedantic \
  -Wno-unused-parameter \
  -I ./src/ \
  -I "$MBEDTLS_DIR/include" \
  -L "$BUILD_DIR" \
  -lmbedtls \
  -lmbedcrypto \
  -lmbedx509 \
  -lembind \
  -s MODULARIZE=1 -s EXPORT_ES6=1 \
  -s ENVIRONMENT='web,worker' \
  -sASSERTIONS=1 \
  -sSTACK_SIZE=8388608 \
  -sEXPORTED_FUNCTIONS=['_js_malloc','_main'] \
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8','setValue']