The Fble Test Suite
===================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Type check and evaluate the given fble expression.
# If the expression is a process type, run the resulting process.
# The test is considered passing if there are no type errors and no undefined
# union field accesses.
# 'modules' is an optional module hierarchy described as a list of 3-tuples of
# name, value, and submodule hierarchy.
proc fble-test { expr modules } { ... }

# Type check and evaluate the given fble expression.
# If the expression is a process type, run the resulting process.
# The test is considered passing if there is a type error or an undefined
# union field access whose reported error is at the location specified.
# loc should be of the form line:col, where line is the line
# number of the expected error and col is the column number within that line
# of the expected error.
# 'modules' is an optional module hierarchy described as a list of 3-tuples of
# name, value, and submodule hierarchy.
proc fble-test-error { loc expr modules } { ... }

# The expr should be a function that takes a single argument of type Nat@ from
# the testing module Nat%.
#
# The test is considered passing if the function's memory use is O(1) in the
# size of the input argument.
proc fble-test-memory-constant { expr } { ... }

# The expr should be a function that takes a single argument of type Nat@ from
# the testing module Nat%.
# The test is considered failing if the function's memory uses is O(1) in the
# size of the input argument.
proc fble-test-memory-growth { expr } { ... }
