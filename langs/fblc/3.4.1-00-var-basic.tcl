set prg {
  # Test a simple variable expression.
  struct Unit();
  union Bool(Unit True, Unit False);

  func main(Bool x ; Bool) {
    x;
  };
}

fblc-test $prg main { Bool:True(Unit()) } "return Bool:True(Unit())"
fblc-test $prg main { Bool:False(Unit()) } "return Bool:False(Unit())"
