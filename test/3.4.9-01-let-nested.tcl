set prg {
  # Test a nested let expression.
  struct Unit();
  struct A(Unit x, Unit y);
  struct A2(A x, A y);
  struct B(Unit w, A2 a, Unit x);

  func main( ; B) {
    Unit u = Unit();
    B(u, { A a = A(u, u); A2(a,a); }, u);
  };
}

fblc-test $prg main {} {
  return B(Unit(),A2(A(Unit(),Unit()),A(Unit(),Unit())),Unit())
}
