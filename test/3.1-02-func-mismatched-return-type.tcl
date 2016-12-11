
# The return type of the function must match the type of the body.
set prg {
  struct Unit();
  struct Donut();

  func main( ; Unit) {
    Donut();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

