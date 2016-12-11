
# Each field of a union should be separated by a comma.
set prg {
  struct Unit();
  union Foo(Unit x, Unit y Unit z);

  func main( ; Foo) {
    Foo:x(Unit());
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

