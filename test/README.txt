The Fable Calvisus Test Suite
=============================

Test cases are described using tcl. Tests are defined using the following
abstract tcl procedures:

# Test that running function or process 'entry' in 'program' with the given
# 'args' and no ports leads to the given 'result'.
proc expect_result { result program entry args } { ... }

# Test that running function or process 'entry' in 'program' with the given
# 'args' and no ports leads to an error indicating the program is malformed.
proc expect_malformed { program entry args } { ... }

# Test that running the process 'entry' in 'program' with given ports and
# arguments leads to the given 'result'.
# The ports should be specified as i:<portid> for input ports and as
# o:<portid> for output ports. The 'script' should be a sequence of commands
# of the form 'put <portid> <value>' and 'get <portid> <value>'. The put
# command causes the value to be written to the given port. The get command
# gets a value from the given port and checks that it is equivalent to the
# given value.
proc expect_proc_result { result program entry ports args script } { ... }

Test cases are organized by numbered sections spec/calvisis.txt.

