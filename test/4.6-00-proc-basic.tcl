
# Test a simple process.

set prg {
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_result Unit() $prg main
skip expect_result_b "" $prg 1

