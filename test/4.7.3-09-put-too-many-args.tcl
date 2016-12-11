
set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // Too many arguments to put.
    myput~(Unit(), Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

