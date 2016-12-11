
set prg {
  struct Unit();

  proc f(Unit ~> myput ; ; Unit) {
    // The variable 'x' is not not declared.
    myput~(x);
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

