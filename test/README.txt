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

Test cases are organized by numbered sections spec/calvisis.txt.

