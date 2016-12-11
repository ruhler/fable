
set prg {
  struct Unit();
  struct A(Unit x, Unit y);
  union Foo(Unit bar, A sludge);

  func main( ; Foo) {
    // No field is provided for the constructor.
    Foo:(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 3

