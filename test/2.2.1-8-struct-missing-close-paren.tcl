
# A struct declaration must have a final close paren.
set prg {
  struct Unit();
  struct Foo(Unit x, Donut y;

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
