set prg {
  // A struct must not be constructed with too many arguments.
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; A) {
    // There should be two arguments provided, not three.
    A(Unit(), Donut(), Unit());
  };
}
# TODO: Should the error be at the A, the open paren, the start of the third
# argument?
fblc-check-error $prg 9:5

