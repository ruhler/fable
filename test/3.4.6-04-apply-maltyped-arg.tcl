
# Test for calling a function with a maltyped argument.

set prg {
  struct Unit();
  struct Donut();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    f(Donut());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

