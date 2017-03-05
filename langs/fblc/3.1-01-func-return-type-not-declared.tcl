set prg {
  struct Unit();

  # The return type Donut is not defined.
  func foo(Unit x ; Donut) {
    Donut();
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:21
