
# Variable definitions must have the right type.

set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Unit y);

  func main( ; A) {
    Unit v = Donut();
    A(v, v);
  };
}

expect_malformed $prg main

