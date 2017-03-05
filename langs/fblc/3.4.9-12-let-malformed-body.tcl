set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    # The body of the let is malformed.
    Unit v = Unit();
    ???;
  };
}
fblc-check-error $prg 8:6
