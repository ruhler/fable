
# Fable README

fble-0.5 ()

Fable is a programming language project. It includes a specification,
reference implementation, library, and sample applications for the fble
programming language.

## Overview

Start with the simplicity of C. Redesign the foundations from the ground up
based on a strong type system and functional programming. Use automatic
memory management and make sure there is a clear way to reason about
performance. Include support for generics and modularity. Remove as many
hard edges, corner cases and redundancies in the language as possible.

The result is a simple, general purpose, statically typed, strict, pure
functional programming language with automatic memory management, support
for polymorphism and modularity, user-defined primitive data types, and a
hint of C-like syntax.

## Source

The source code for fble is available from github at
<https://github.com/ruhler/fable>.

## Documentation

* **Fble Language Specification**: [spec/fble.fbld](spec/fble.fbld)

* **Fble Style Guide**: [spec/style.fbld](spec/style.fbld)

* **Fbld Document Markup Specification**: [fbld/fbld.fbld](fbld/fbld.fbld)

* **Tutorials**: [tutorials/Tutorials.fbld](tutorials/Tutorials.fbld)

* **Release Notes**: [Release.fbld](Release.fbld)

The latest generated documentation is also available online at
<https://ruhler.github.io/fable>.

## Directory Structure

* **bin/**: Source code for fble compiler binaries.

* **book/**: Drafts for a book about the design of the fble language.

* **fbld/**: A lightweight document markup language used for documenting fble.

* **include/**: Public header files for the fble library.

* **lib/**: Source code for an fble interpreter and compiler implemented in C.

* **pkgs/**: Sample fble library and application packages.

* **spec/**: Specification of the fble language, including spec tests.

* **test/**:  Source code for fble test binaries and other test utilities.

* **thoughts/**: A collection of running discussions about fble.

* **tutorials/**: Fble language tutorials.

* **vim/**: Vim syntax files for fbld and fble.

## Build Requirements

To build and run the tests for all of fble, the following software is
required. The version numbers provided are versions that are known to work:

* **Expect 5.45.4**: To test fble debugging.

* **GNU Binutils for Debian 2.31.1**: For assembling compiled fble programs.

* **GNU Bison 3.3.2**: For generating the fble parser.

* **GNU Compiler Collection (Debian 8.3.0-6) 8.3.0**: For compiling fble.

* **GNU coreutils 8.30**: Used in the build system.

* **GNU Debugger (Debian 8.2.1-2+b3) 8.2.1**: Used to test fble debugging.

* **GNU diffutils 3.7**: For some test cases.

* **GNU grep 3.3**: For some test cases.

* **GNU groff version 1.22.4**: For formatting help usage text.

* **Ninja 1.10.1**: For the build system.

* **Open GL 1.2**: For graphical fble apps.

* **Simple DirectMedia Layer 2.0.9**: For graphical fble apps.

* **Tcl 8.6**: For generating build ninja files.

To install required dependencies on a debian based system:

    apt install \
      expect binutils bison coreutils \
      gcc gdb diffutils grep groff-base \
      ninja-build libgl-dev libsdl2-dev tcl8.6

To install required dependencies on an MSYS2 UCRT64 based system:

     pacman -S tcl ninja gcc bison groff diffutils \
       mingw-w64-ucrt-x86_64-toolchain \
       mingw-w64-ucrt-x86_64-SDL2 \
       mingw-w64-ucrt-x86_64-mesa
     export MSYS2_ARG_CONV_EXCL='*'

## Build

To build and install:

    ./configure --prefix=/usr/local && ninja && ninja install

Additional ninja targets:

* **tests**: Runs tests on uninstalled binaries.

* **www**: Builds html documentation.

* **check**: Builds and tests everything without installing anything.

## Vim Files

Vim ftplugin, syntax, and indent files for the fble language are available
in the `vim/` directory. You can run the following commands to install these
files:

    mkdir -p ~/.vim/ftdetect ~/.vim/ftplugin ~/.vim/indent ~/.vim/syntax
    cp vim/ftdetect/* ~/.vim/ftdetect
    cp vim/ftplugin/* ~/.vim/ftplugin
    cp vim/indent/* ~/.vim/indent
    cp vim/syntax/* ~/.vim/syntax
