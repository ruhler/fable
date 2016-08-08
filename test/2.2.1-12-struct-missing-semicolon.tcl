

# A struct declaration must have a final close paren.
set prg {
  struct Unit();
  struct Foo(Unit x, Unit y)

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
