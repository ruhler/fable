
# Test for calling a function with a maltyped argument.

set prg {
  struct Unit();
  struct Donut();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    f(Donut());
  };
}

fblc-check-error $prg

