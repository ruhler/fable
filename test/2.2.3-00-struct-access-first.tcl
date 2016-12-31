set prg {
  # Accessing the first component of a struct.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Unit) {
    .x(A(Unit(), Donut()));
  };
}
expect_result Unit() $prg main
expect_result_b 0 $prg 3
