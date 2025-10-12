#!/bin/sh
# Wraps a file as an fble string literal.

echo "/Core/String/Ascii%.Str|'"
cat $1
echo "';"
