
# A union literal must be for a declared union type.

set prg {
  struct Unit();

  func main( ; Unit) {
    Foo:bar(Unit()).bar;
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

