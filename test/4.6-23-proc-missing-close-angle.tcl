set prg {
  struct Unit();

  // The '>' character is missing in the second port polarity specifier.
  proc p(Unit <~ px, Unit ~ py ; Unit x, Unit y; Unit) {
    $(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

