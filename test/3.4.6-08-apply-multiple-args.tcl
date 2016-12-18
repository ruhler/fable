

# Test that we can call a function with multiple arguments.
set prg {
  struct Unit();
  struct Pair(Unit a, Unit b);

  func foo(Unit a, Unit b ; Pair) {
    Pair(a, b);
  };

  func main( ; Pair) {
    foo(Unit(), Unit());
  };
}
expect_result Pair(Unit(),Unit()) $prg main
expect_result_b "" $prg 3

