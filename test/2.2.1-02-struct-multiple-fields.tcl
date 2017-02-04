
set prg {
  # A struct can be declared that contains multiple fields.
  struct Unit();
  struct Donut();
  struct MultiField(Unit x, Donut y);

  func main( ; MultiField) {
    MultiField(Unit(), Donut());
  };
}

expect_result MultiField(Unit(),Donut()) $prg main
