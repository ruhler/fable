set prg {
  struct Unit();

  proc main( ; ; Unit) {
    // The argument to eval has the wrong syntax.
    $(???);
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

