The Fble Test Suite
===================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Type check and evaluate the given fble expression.
# If the expression is a process type, run the resulting process.
# The test is considered passing if there are no type errors and no undefined
# union field accesses.
proc fble-test { expr } { ... }

# Type check and evaluate the given fble expression.
# If the expression is a process type, run the resulting process.
# The test is considered passing if there is a type error or an undefined
# union field access whose reported error is at the location specified.
# loc should be of the form line:col, where line is the line
# number of the expected error and col is the column number within that line
# of the expected error.
proc fble-test-error { loc expr } { ... }

