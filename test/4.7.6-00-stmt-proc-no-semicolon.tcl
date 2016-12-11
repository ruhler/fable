set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // Missing a semicolon.
    $(Unit())
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

