set prg {
  # A union literal must be for a declared union type.
  struct Unit();

  func main( ; Unit) {
    .bar(Foo:bar(Unit()));
  };
}
fblc-check-error $prg 6:10
