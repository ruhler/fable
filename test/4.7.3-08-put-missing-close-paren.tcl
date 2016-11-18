
set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // The close parenthesis is missing.
    myput~(Unit();
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main

