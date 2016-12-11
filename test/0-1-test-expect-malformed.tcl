
# Test a basic 'expect_malformed' test.

set prg { X X X X X X X }

expect_malformed $prg main
expect_malformed_b $prg 0

