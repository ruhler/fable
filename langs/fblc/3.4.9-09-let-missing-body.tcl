set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    # The body of the let is missing.
    Unit v = Unit();
  };
}
fblc-check-error $prg 8:3
