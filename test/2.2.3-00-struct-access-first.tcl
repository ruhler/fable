# Accessing the first component of a struct.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    A(Unit(), Donut()).x;
  };
}
expect_result Unit() $prg main
skip expect_result_b "" $prg 3

