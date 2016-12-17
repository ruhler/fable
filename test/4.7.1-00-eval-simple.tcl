set prg {
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_result Unit() $prg main
expect_result_b "" $prg 1

