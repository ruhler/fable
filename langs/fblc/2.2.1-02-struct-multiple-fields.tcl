set prg {
  # A struct can be declared that contains multiple fields.
  struct Unit();
  struct Donut();
  struct MultiField(Unit x, Donut y);

  func main( ; MultiField) {
    MultiField(Unit(), Donut());
  };
}

fblc-test $prg main {} "return MultiField(Unit(),Donut())"
