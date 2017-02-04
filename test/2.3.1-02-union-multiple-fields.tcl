set prg {
  # A union can be declared that contains multiple fields.
  struct Unit();
  struct Donut();
  union MultiField(Unit x, Donut y);

  func main( ; MultiField) {
    MultiField:x(Unit());
  };
}

fblc-test $prg main {} "return MultiField:x(Unit())"
