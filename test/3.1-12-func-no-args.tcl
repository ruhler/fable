
# Test a function with no inputs arguments.

set prg {
  struct Unit();

  func main( ; Unit) {
    Unit();
  };
}

expect_result Unit() $prg main
