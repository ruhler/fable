set prg {
  # Accessing the first component of a struct.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    A(Unit(), Donut()).x;
  };
}

fblc-test $prg main {} "return Unit()"
