#!/bin/sh

# Ensure -mprofile-kernel obeys the no_instrument_function attribute

$@ -pg -mprofile-kernel -c -x c /dev/null -o /dev/null 2> /dev/null || exit

echo "void __attribute__((no_instrument_function)) zot(void) { }" | $@ -pg -mprofile-kernel -x c - -S -o- | grep -q mcount || echo "y"
