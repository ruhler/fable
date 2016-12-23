
# The return type of a function must be defined.
set prg {
  struct Unit();

  func foo(Unit x ; Donut) {
    Donut();
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg

