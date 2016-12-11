
# The object of a field access must be well formed.
set prg {
  struct Unit();
  struct A(Unit x, Unit y);

  func main( ; Unit) {
    // The variable 'x' has not  been declared.
    x.y;
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2
