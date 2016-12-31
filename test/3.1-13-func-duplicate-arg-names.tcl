set prg {
  # Function declarations must have unique argument names.
  struct Unit();

  func f(Unit x, Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
fblc-check-error $prg 5:23
