
set prg {
  struct Unit();
  struct Donut();

  proc f(Unit ~> myput ; ; Unit) {
    // The argument to myput has the wrong type.
    myput~(Donut());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

