set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // The variable x is not in scope.
    $(x);
  };
}

expect_malformed $prg main

