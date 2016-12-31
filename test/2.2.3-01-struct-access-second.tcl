set prg {
  # Accessing the second component of a struct.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Donut) {
    .y(A(Unit(), Donut()));
  };
}
expect_result Donut() $prg main
expect_result_b 0 $prg 3
