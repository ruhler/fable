

# Declared functions must have a name.
set prg {
  struct Unit();

  func (Unit x, Unit y; Unit) {
    x;
  };

  func main( ; Unit) {
    Unit();
  };
}
expect_malformed $prg main
expect_malformed_b $prg 2

