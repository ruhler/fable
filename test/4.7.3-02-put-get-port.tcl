
set prg {
  struct Unit();

  proc f(Unit <~ myput ; ; Unit) {
    // myput port has the wrong polarity.
    myput~(Unit());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

