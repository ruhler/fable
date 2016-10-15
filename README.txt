
Directory Structure
-------------------
fblc/ - Source code for an fblc interpreter implemented in C.
out/ - Directory where build outputs are placed, created as needed.
prgms/ - Directory containing sample fblc programs.
spec/ - Source for the specification of the fable languages.
test/ - A test suite for fblc.
util/ - Miscellaneous utilities for fblc, including a vim syntax file.

Building Fblc
-------------
1. To build and test fblc, run:

  $ make

The results are placed in the out/ directory. The fblc executable is out/fblc.

2. To build the calvisus specification, run:

  $ make doc

The result is placed at out/calvisus.html.

