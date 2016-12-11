
# Test for calling a function with too many arguments.

set prg {
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    f(Unit(), Unit());
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2
