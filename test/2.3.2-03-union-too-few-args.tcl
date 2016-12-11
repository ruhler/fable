
set prg {
  struct Unit();
  union Foo(Unit bar);

  func main( ; Foo) {
    // The constructor is missing its argument.
    Foo:bar();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

