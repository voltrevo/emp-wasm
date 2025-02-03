#!/bin/bash

ARG1=$1

set -euo pipefail

# Variables
MBEDTLS_DIR="./external/mbedtls"
BUILD_DIR="$MBEDTLS_DIR/build/library"

if [ ! -d "$BUILD_DIR" ]; then
  echo "Please run ./scripts/build_mbedtls.sh first"
  exit 1
fi

mkdir -p build

CONDITIONAL_OPTS=""

if [ "$ARG1" == "" ]; then
  CONDITIONAL_OPTS="-O3"
elif [ "$ARG1" == "debug" ]; then
  CONDITIONAL_OPTS="-g" # FIXME: want -D__debug but it doesn't work
else
  echo "Invalid argument"
  exit 1
fi

# Emscripten build
em++ programs/jslib.cpp -sASYNCIFY -o build/jslib.js \
  $CONDITIONAL_OPTS \
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
  -sNO_DISABLE_EXCEPTION_CATCHING \
  -sASSERTIONS=1 \
  -sSTACK_SIZE=8388608 \
  -sASYNCIFY_STACK_SIZE=16384 \
  -sEXPORTED_FUNCTIONS=['_js_malloc','_main'] \
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8','setValue'] \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s EXPORT_NAME=createModule
