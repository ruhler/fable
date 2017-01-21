
# Test a basic 'expect_malformed' test.
set prg { X X X X X X X }

fblc-check-error $prg 1:6

