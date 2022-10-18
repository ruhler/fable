# Fable

Fable is a programming language project. It includes a specification,
referencing implementation, library, and sample applications for the fble
programming language.

## Overview

Fable is a hobby project to design my own programming language. My goal is to
design a general purpose programming language good enough to be my preferred
programming language.

## Directory Structure

spec/ - Specification of the fble language, including spec tests.
fble/ - Source code for an fble interpreter and compiler implemented in C.
pkgs/ - Sample fble library and application packages.
thoughts/ - A collection of running discussions about fble.
docs/ - A collection of user guides.
book/ - Another collection of discussions about fble.
out/ - Directory where build outputs are placed, created as needed.

## Build Requirements

To build and run the tests for Fable, the following software is required. The
version numbers provided are versions that are known to work:

* GNU Make 4.2.1 - For convenience launching build and test.
* Ninja 1.10.1 - For the build system.
* Tcl 8.6 - For generating build ninja files.
* GNU Compiler Collection (Debian 8.3.0-6) 8.3.0 - For compiling fble.
* GNU Binutils for Debian 2.31.1 - For assembling compiled fble programs.
* GNU Bison 3.3.2 - For generating the fble parser.
* GNU Debugger (Debian 8.2.1-2+b3) 8.2.1 - Used to test fble debugging.
* Tcl Expect 8.6 - Used to test fble debugging.
* Simple DirectMedia Layer 2.0.9 - For graphical fble apps.
* Open GL 1.2 - For graphical fble apps.
* GNU grep 3.3 - Used for some test cases.
* GNU diffutils 3.7 - Used for some test cases.

## Build and Test

To build and test everything, run:

  $ make

The results are placed in the out/ directory.

## Vim Files

Vim ftplugin, syntax, and indent files for the fble language are available in
the spec/vim directory. Consult the vim documentation for how to install if
desired.
  
