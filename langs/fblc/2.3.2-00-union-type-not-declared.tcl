set prg {
  # A union literal must be for a declared union type.
  struct Unit();

  func main( ; Unit) {
    Foo:bar(Unit()).bar;
  };
}
fblc-check-error $prg 6:5
