
set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // The open parenthesis is missing.
    myput~ Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

