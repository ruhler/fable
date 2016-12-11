
# A union must have at least one field.

set prg {
  struct Unit();
  union NoFields();

  func main( ; Unit) {
    Unit();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

