
# A union literal must be for a declared union type.

set prg {
  struct Unit();

  func main( ; Unit) {
    Foo:bar(Unit()).bar;
  };
}

fblc-check-error $prg

