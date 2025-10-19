#!/bin/sh
# Wraps a file as an fble string literal.

echo "/Std/Char/Ascii%.Str|'"
cat $1
echo "';"
