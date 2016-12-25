
# A union can be declared that contains multiple fields.

set prg {
  struct Unit();
  struct Donut();
  union MultiField(Unit x, Donut y);

  func main( ; MultiField) {
    MultiField:x(Unit());
  };
}

expect_result MultiField:x(Unit()) $prg main
expect_result_b 0 $prg 3

