
# A struct must not be constructed with too many arguments.
set prg {
  struct Unit();
  struct Donut();
  struct A(Unit x, Donut y);

  func main( ; A) {
    // There should be two arguments provided, not three.
    A(Unit(), Donut(), Unit());
  };
}
expect_malformed $prg main

