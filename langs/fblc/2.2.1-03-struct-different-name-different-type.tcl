
# Two structs with different names are considered different types, even if
# they have the same fields.

set prg {
  struct Unit();
  struct Donut();

  func main( ; Unit) {
    Donut();
  };
}

# TODO: Should the error be at the open brace on 5:22 instead?
fblc-check-error $prg 6:5

set prg {
  struct Unit();

  struct A(Unit x, Unit y);
  struct B(Unit x, Unit y);

  func main( ; A) {
    B(Unit(), Unit());
  };
}

fblc-check-error $prg 8:5
