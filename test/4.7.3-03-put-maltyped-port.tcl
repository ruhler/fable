
set prg {
  struct Unit();
  struct Donut();

  proc f(Donut ~> myput ; ; Unit) {
    // myput port has the wrong type.
    myput~(Donut());
  };

  proc main( ; ; Unit) {
    $(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

