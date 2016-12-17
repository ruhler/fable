
# Test that we can still refer to variables in scope after a function call.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);
  struct B(Unit w, A a, Unit z);

  func foo(Unit v ; A) {
    A(v, Donut());
  };

  /// Expect: B(Unit(),A(Unit(),Donut()),Unit())
  func main( ; B) {
    Unit v = Unit();
    B(v, foo(v), v);
  };
}
expect_result B(Unit(),A(Unit(),Donut()),Unit()) $prg main
expect_result_b "" $prg 5

