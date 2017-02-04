The Fable Calvisus Test Suite
=============================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Test that running function or process 'entry' in 'program' with the given
# 'args' and no ports leads to the given 'result'.
proc expect_result { result program entry args } { ... }

# Test that the given fblc text program is malformed, and that the error is
# located at loc. loc should be of the form line:col, where line is the line
# number of the expected error and col is the column number within that line
# of the expected error.
proc fblc-check-error { program loc } {

# Test that running the process 'entry' in 'program' with given ports and
# arguments leads to the given 'result'.
# The 'script' should be a sequence of commands of the form 'put <portid>
# <value>' and 'get <portid> <value>'. The put command causes the value to be
# written to the given port. The get command gets a value from the given port
# and checks that it is equivalent to the given value.
proc fblc-test { result program entry args script } { ... }

Test cases are organized by numbered sections spec/calvisis.txt.

