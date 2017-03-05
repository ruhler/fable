set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    # The field name is missing.
    A(Unit(), Donut()).;
  };
}
fblc-check-error $prg 8:24
