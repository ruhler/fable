set prg {
  # Field access must be used with a name, not an expression.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);
  union Bool(Unit true, Unit false);

  func main( ; Unit) {
    # You can't access a struct with a conditional expression.
    .?(Bool:true(Unit() ; Unit(), Unit()))(A(Unit(), Donut()));
  };
}
fblc-check-error $prg 10:6
