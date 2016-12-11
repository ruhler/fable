
# Test for calling a function that has not been declared.

set prg {
  struct Unit();

  func main( ; Unit) {
    f(Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 1

