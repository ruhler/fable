set prg {
  struct Unit();

  # The type Donut is not defined.
  func f(Unit x, Donut y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:18
