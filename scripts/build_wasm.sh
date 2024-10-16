#!/bin/bash

set -euo pipefail

# Variables
MBEDTLS_DIR="./external/mbedtls"
BUILD_DIR="$MBEDTLS_DIR/build/library"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Please run ./scripts/build_mbedtls.sh first"
  exit 1
fi

mkdir -p build

# Emscripten build
em++ programs/jslib.cpp -sASYNCIFY -o build/jslib.js \
  -O3 \
  -Wall \
  -Wextra \
  -pedantic \
  -Wno-unused-parameter \
  -I ./src/cpp/ \
  -I "$MBEDTLS_DIR/include" \
  -L "$BUILD_DIR" \
  -lmbedtls \
  -lmbedcrypto \
  -lmbedx509 \
  -lembind \
  -s SINGLE_FILE=1 \
  -s ENVIRONMENT='web,worker,node' \
  -sASSERTIONS=1 \
  -sSTACK_SIZE=8388608 \
  -sEXPORTED_FUNCTIONS=['_js_malloc','_main'] \
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8','setValue']

# -s MODULARIZE=1 -s EXPORT_ES6=1 \
# -s EXPORT_NAME=EmscriptenModule \