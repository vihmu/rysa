#!/bin/sh
if command -v cc; then
  cc -o rysa rysa.c -std=c99 -Wall -Wextra -Wpedantic -O2
fi
