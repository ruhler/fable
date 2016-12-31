set prg {
  // The return type of the function must match the type of the body.
  struct Unit();
  struct Donut();

  func main( ; Unit) {
    Donut();
  };
}
fblc-check-error $prg 7:5
