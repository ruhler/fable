
# Types passed to a struct constructor must have the right type.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; A) {
    // The second argument to A should be of type Donut, not Unit.
    A(Unit(), Unit());
  };
}
expect_malformed $prg main

