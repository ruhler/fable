
set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // myget port is not in scope.
    myget~();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

