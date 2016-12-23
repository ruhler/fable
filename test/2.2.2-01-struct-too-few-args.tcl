
# A struct must be constructed with sufficient number of arguments.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; A) {
    // There should be two arguments provided, not one.
    A(Unit());
  };
}
fblc-check-error $prg

