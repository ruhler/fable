set prg {
  // A union literal must be for a valid field of the union.
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Foo) {
    // Foo has no field called "blah".
    Foo:blah(Unit());
  };
}
fblc-check-error $prg 9:9
