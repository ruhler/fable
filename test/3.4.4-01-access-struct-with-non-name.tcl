
# Field access must be used with a name, not an expression.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);
  union Bool(Unit true, Unit false);

  func main( ; Unit) {
    // You can't access a struct with a conditional expression.
    A(Unit(), Donut()).?(Bool:true(Unit() ; Unit(), Unit()));
  };
}

fblc-check-error $prg
