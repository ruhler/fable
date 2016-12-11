
# Two structs with different names are considered different types, even if
# they have the same fields.

set prg {
  struct Unit();
  struct Donut();

  func main( ; Unit) {
    Donut();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

set prg {
  struct Unit();

  struct A(Unit x, Unit y);
  struct B(Unit x, Unit y);

  func main( ; A) {
    B(Unit(), Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3
