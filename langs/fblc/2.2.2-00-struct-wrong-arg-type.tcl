set prg {
  # Types passed to a struct constructor must have the right type.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; A) {
    # The second argument to A should be of type Donut, not Unit.
    A(Unit(), Unit());
  };
}
fblc-check-error $prg 9:15
