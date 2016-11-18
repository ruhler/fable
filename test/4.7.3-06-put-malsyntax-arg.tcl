
set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // The argument to the port has invalid syntax.
    myput~(???);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main

