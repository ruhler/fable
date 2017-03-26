set prg {
  # Two structs with different names are considered different types, even if
  # they have the same fields.
  struct Unit();
  struct Donut();

  func main( ; Unit) {
    Donut();
  };
}

fblc-check-error $prg 8:5

set prg {
  struct Unit();

  struct A(Unit x, Unit y);
  struct B(Unit x, Unit y);

  func main( ; A) {
    B(Unit(), Unit());
  };
}

fblc-check-error $prg 8:5
