set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // The open parenthesis is missing
    $ Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

