#!/bin/sh
# Wraps a file as an fble string literal.

echo "/Core/String%.Str|'"
cat $1
echo "';"
