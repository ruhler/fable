The Fable Damocrates Test Suite
===============================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Test running the process 'entry' in 'program' with given arguments.
# The program is specified as a list of alternating file name, file content
# elements, such as {Foo.mdecl { mdecl Foo...} Foo.mdefn { mdefn Foo...}}
# The entry should be a qualified function, such as 'Foo@main'.
# The 'script' should be a sequence of commands of the form
# 'put <port> <value>', 'get <portid> <value>', and 'return <value>'.
# The put command causes the value to be written to the given port. The get
# command gets a value from the given port and checks that it is equivalent to
# the given value. The return command checks that the process has returned a
# result equivalent to the given value.
proc fbld-test { program entry args script } { ... }

