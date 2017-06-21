The Fable Damocrates Test Suite
===============================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Test that the given fbld module is malformed, and that the error is
# located at loc. loc should be of the form file:line:col, where file is the
# base name of the file containing the error, line is the line # number of the
expected error and col is the column number within that line # of the expected
error.
proc fbld-check-error { program module loc } { ... }

# Test running the process 'entry' in 'program' with given arguments.
# The program is specified as a list of alternating file name, file content
# elements, such as {Foo.mdecl { mdecl Foo...} Foo.mdefn { mdefn Foo...}}
# The entry should be a qualified function, such as 'main@Foo'.
# The 'script' should be a sequence of commands of the form
# 'put <port> <value>', 'get <portid> <value>', and 'return <value>'.
# The put command causes the value to be written to the given port. The get
# command gets a value from the given port and checks that it is equivalent to
# the given value. The return command checks that the process has returned a
# result equivalent to the given value.
proc fbld-test { program entry args script } { ... }

