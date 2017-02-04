
# Verify that a variable refers to the proper value.
set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func main(Bool x, Bool y; Bool) {
    y;
  };
}

expect_result Bool:True(Unit()) $prg main Bool:False(Unit()) Bool:True(Unit())
expect_result Bool:False(Unit()) $prg main Bool:True(Unit()) Bool:False(Unit())
