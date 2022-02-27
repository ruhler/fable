The Fble Test Suite
===================
The SpecTests directory contains a collection of fble modules used to test
that an implementation of the fble language satisfies the specification. Those
modules that contain @@fble-test@@ in a comment in the file indicate a test
case, with the following arguments describing the expected behavior when
running the test.

The following @@fble-test@@ arguments are supported for describing test
cases:

No Error
--------
Annotation:  # @@fble-test@@ no-error

Type check and evaluate the module. If the module is a process type, run the
resulting process. The test is considered passing if there are no type errors
and no undefined union field accesses.

Compile Error
-------------
Annotation: # @@fble-test@@ compile-error <loc>

Compile the module. The test is considered passing if there is a compilation
error reported at location <loc>. The location of the error message specified
in the form line:col, where line is the line number of the expected error and
col is the column number in the line of the expected error.

Runtime Error
-------------
Annotation: # @@fble-test@@ runtime-error <loc>

Compile and evaluate the module. If the module is a process type, run the
resulting process. The test is considered passing if there is a runtime error
reported at location <loc>.

Memory Constant
---------------
Annotation: # @@fble-test@@ memory-constant

The module value should be a function that takes a single argument of type
/SpecTests/Nat%.Nat@. The test is considered passing if the function's memory
use is O(1) in the value of the input argument.

Memory Growth
-------------
Annotation: # @@fble-test@@ memory-growth

The module value should be a function that takes a single argument of type
/SpecTests/Nat%.Nat@. The test is considered failing if the function's memory
use is O(1) in the value of the input argument.

None
----
Annotation: # @@fble-test@@ none

This annotation can be used to indicate a .fble file that is not intended to
be used as a top level test case. For example, the .fble file may be a
supporting file referenced from another test.
