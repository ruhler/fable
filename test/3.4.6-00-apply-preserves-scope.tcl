set prg {
  # Test that we can still refer to variables in scope after a function call.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);
  struct B(Unit w, A a, Unit z);

  func foo(Unit v ; A) {
    A(v, Donut());
  };

  func main( ; B) {
    Unit v = Unit();
    B(v, foo(v), v);
  };
}
expect_result B(Unit(),A(Unit(),Donut()),Unit()) $prg main
