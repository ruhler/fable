set prg {
  struct Unit();

  func f(Unit x; Unit) {
    x;
  };

  func main( ; Unit) {
    // The argument has invalid syntax.
    f(???);
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

