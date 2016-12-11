
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Foo) {
    // The bar field should have type Unit, not A.
    Foo:bar(A(Unit(), Unit()));
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

