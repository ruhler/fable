set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; A) {
    // The variable definition is missing.
    Unit v = ;
    A(v, v);
  };
}
fblc-check-error $prg 7:14
