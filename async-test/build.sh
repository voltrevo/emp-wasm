#!/bin/bash

em++ async.cpp -sASYNCIFY -o index.html \
  -sEXPORTED_FUNCTIONS=['_js_malloc','_js_free','_main'] \
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8','setValue']
