set prg {
  struct Unit();
  struct Pair(Unit first, Unit second);

  # Test that the order functions are declared does not matter.
  func a( ; Unit) {
    Unit();
  };

  func b( ; Unit) {
    c(a()).first;
  };

  func c(Unit x; Pair) {
    Pair(x, x);
  };

  func main( ; Unit) {
    b();
  };
}

fblc-test $prg main {} "return Unit()"

