set prg {
  # Test a basic let expression.
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    Unit v = Unit();
    A(v, v);
  };
}

fblc-test $prg main {} "return A(Unit(),Unit())"
