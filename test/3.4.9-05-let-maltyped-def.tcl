set prg {
  // Variable definitions must have the right type.
  struct Unit();
  struct Donut();
  struct A(Unit x, Unit y);

  func main( ; A) {
    Unit v = Donut();
    A(v, v);
  };
}
fblc-check-error $prg 8:14
