set prg {
  struct Unit();

  func f<struct S(Unit a, Unit b)>( ; Unit) {
    S(Unit(), Unit()).b;
  };

  struct UnitPair(Unit a, Unit b);

  func main( ; Unit) {
    # UnitPair is a struct, matching as required.
    f<UnitPair>();
  };
}

fbld-test $prg main {} {
  return Unit()
}
