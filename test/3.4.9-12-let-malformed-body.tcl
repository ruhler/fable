
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The body of the let is malformed.
    Unit v = Unit();
    ???;
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

