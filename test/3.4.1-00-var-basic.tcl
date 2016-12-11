
# Test a simple variable expression.

set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func main(Bool x ; Bool) {
    x;
  };
}

expect_result Bool:True(Unit()) $prg main Bool:True(Unit())
expect_result Bool:False(Unit()) $prg main Bool:False(Unit())
skip expect_result_b "0" $prg 2 "0"
skip expect_result_b "1" $prg 2 "1"

