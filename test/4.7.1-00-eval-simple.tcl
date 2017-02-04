set prg {
  struct Unit();

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_result Unit() $prg main
