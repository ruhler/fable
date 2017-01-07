set prg {
  # Test the most basic 'expect_result' test.
  struct Unit();

  func main( ; Unit) {
    Unit();
  };
}

fblc-test Unit() $prg main {} {}
expect_result Unit() $prg main
expect_result_b 0 $prg 1
