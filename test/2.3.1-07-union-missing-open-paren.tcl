
# A union declaration must have an open paren.
set prg {
  struct Unit();
  union Foo Unit x, Donut y);

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2
