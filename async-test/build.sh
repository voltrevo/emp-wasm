#!/bin/bash

em++ async.cpp Buffer.cpp -sASYNCIFY -o index.html \
  -I ../src/ \
  -I $(brew --prefix openssl)/include \
  -L $(brew --prefix openssl)/lib \
  -lcrypto \
  -lssl \
  -lembind \
  -s MODULARIZE=1 -s EXPORT_ES6=1 \
  -s ENVIRONMENT='web,worker' \
  -sEXPORTED_FUNCTIONS=['_js_malloc','_main'] \
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8','setValue']
