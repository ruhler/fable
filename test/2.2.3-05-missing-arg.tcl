set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    # The argument is missing.
    .x();
  };
}
fblc-check-error $prg 8:8
