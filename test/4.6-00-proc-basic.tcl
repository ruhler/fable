
# Test a simple process.

set prg {
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_result Unit() $prg main
expect_result_b 0 $prg 1

