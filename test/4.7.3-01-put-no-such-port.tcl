
set prg {
  struct Unit();

  proc f( ; ; Unit) {
    // myput port is not in scope.
    myput~(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main

