#!/bin/sh
# Wraps a file as an fble string literal.

echo "/Core/Char/Ascii%.Str|'"
cat $1
echo "';"
