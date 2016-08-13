# Accessing the second component of a struct.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; Donut) {
    A(Unit(), Donut()).y;
  };
}
expect_result Donut() $prg main

