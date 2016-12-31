# Test the most basic 'expect_result' test.
set prg {
  struct Unit();

  # Including one with a comment in it.
  func main( ; Unit) {
    Unit();
  };
}

expect_result Unit() $prg main
expect_result_b 0 $prg 1
