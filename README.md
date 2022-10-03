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

## Requirements

TODO:
* make, ninja, libsdl, tcl, gcc, as
* aarch64
* others?

## Build and Test

To build and test everything, run:

  $ make

The results are placed in the out/ directory.

