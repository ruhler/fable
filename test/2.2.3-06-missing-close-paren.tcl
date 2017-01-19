set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    # The close parenthesis is missing.
    .x(A(Unit(), Donut());
  };
}
fblc-check-error $prg 8:26
