#!/bin/sh
# Generates version.fbld output given the current fbld version number.

echo "@let[FbleVersion][$1] @@"
echo "@let[FbleVersionStamp][@FbleVersion (@BuildStamp)] @@"
