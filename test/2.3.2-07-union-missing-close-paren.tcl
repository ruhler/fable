
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Foo) {
    // The close paren is missing.
    Foo:bar(Unit();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

