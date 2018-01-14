The Fable Damocrates Test Suite
===============================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Test that the given fbld text program is malformed, and that the error is
# located at loc. loc should be of the form line:col, where line is the line
# number of the expected error and col is the column number within that line
# of the expected error.
proc fbld-check-error { program loc } { ... }

# Test running the process 'entry' in 'program' with given arguments.
# The 'script' should be a sequence of commands of the form
# 'put <port> <value>', 'get <portid> <value>', and 'return <value>'.
# The put command causes the value to be written to the given port. The get
# command gets a value from the given port and checks that it is equivalent to
# the given value. The return command checks that the process has returned a
# result equivalent to the given value.
proc fbld-test { program entry args script } { ... }

