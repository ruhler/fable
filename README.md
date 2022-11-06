# Fable

Fable is a programming language project. It includes a specification,
referencing implementation, library, and sample applications for the fble
programming language.

## Overview

Fable is a hobby project to design my own programming language. My goal is to
design a general purpose programming language good enough to be my preferred
programming language.

## Directory Structure

* bin/ - Source code for fble compiler binaries.
* book/ - Drafts for a book about the design of the fble language.
* include/ - Public header files for the fble library.
* lib/ - Source code for an fble interpreter and compiler implemented in C.
* pkgs/ - Sample fble library and application packages.
* spec/ - Specification of the fble language, including spec tests.
* test/ - Source code for fble test binaries and other test utilities.
* thoughts/ - A collection of running discussions about fble.
* tutorials/ - Tutorials for getting started with fble.

## Build Requirements

To build and run the tests for Fable, the following software is required. The
version numbers provided are versions that are known to work:

* GNU Binutils for Debian 2.31.1 - For assembling compiled fble programs.
* GNU Bison 3.3.2 - For generating the fble parser.
* GNU Compiler Collection (Debian 8.3.0-6) 8.3.0 - For compiling fble.
* GNU Debugger (Debian 8.2.1-2+b3) 8.2.1 - Used to test fble debugging.
* GNU diffutils 3.7 - For some test cases.
* GNU grep 3.3 - For some test cases.
* GNU help2man 1.47.8 - For generating man pages.
* Ninja 1.10.1 - For the build system.
* Open GL 1.2 - For graphical fble apps.
* Simple DirectMedia Layer 2.0.9 - For graphical fble apps.
* Tcl 8.6 - For generating build ninja files.
* Tcl Expect 8.6 - To test fble debugging.

## Build and Test

To build:

    $ mkdir build
    $ cd build
    $ ../configure --prefix=/usr/local
    $ ninja

To run tests:

    $ ninja check

To install:

    $ ninja install

## Vim Files

Vim ftplugin, syntax, and indent files for the fble language are available in
the spec/vim directory. Consult the vim documentation for how to install if
desired.
  
## Next Steps

See tutorials/Tutorial1.md for the first in a series of tutorials about the
fble language to get started.
