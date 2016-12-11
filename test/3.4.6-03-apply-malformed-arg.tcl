
# Test for calling a function with a malformed argument.

set prg {
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    // The variable 'x' has not been declared.
    f(x);
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

