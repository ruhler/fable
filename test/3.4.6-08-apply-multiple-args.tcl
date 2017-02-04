set prg {
  # Test that we can call a function with multiple arguments.
  struct Unit();
  struct Pair(Unit a, Unit b);

  func foo(Unit a, Unit b ; Pair) {
    Pair(a, b);
  };

  func main( ; Pair) {
    foo(Unit(), Unit());
  };
}

fblc-test $prg main {} {
  return Pair(Unit(),Unit())
}
