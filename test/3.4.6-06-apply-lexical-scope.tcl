
# Test that variables in the calling scope are not visible to the function.

set prg {
  struct Unit();

  func f(Unit x ; Unit) {
    y;
  };

  func main( ; Unit) {
    Unit y = Unit();
    f(Unit());
  };
}

expect_malformed $prg main

