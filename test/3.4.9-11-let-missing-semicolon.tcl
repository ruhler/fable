set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The semicolon after the declaration is missing.
    Unit v = Unit()
    A(v, v);
  };
}
fblc-check-error $prg 8:5
