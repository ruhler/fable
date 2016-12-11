
# A struct declaration must have a name.
set prg {
  struct Unit();
  struct (Unit x, Unit y);

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2
