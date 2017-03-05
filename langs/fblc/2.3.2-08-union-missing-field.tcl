set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Foo) {
    # No field is provided for the constructor.
    Foo:(Unit());
  };
}
fblc-check-error $prg 8:9
