#!/bin/bash

em++ async.cpp Buffer.cpp -sASYNCIFY -o index.html \
  -sEXPORTED_FUNCTIONS=['_js_malloc','_main'] \
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8','setValue']
