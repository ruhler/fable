set prg {
  struct Unit();
  union Bool(Unit True, Unit False);

  func main(Bool x, Bool y; Bool) {
    x;   # x is not the most recent variable on the stack.
  };
}

fblc-test $prg main { Bool:False(Unit()) Bool:True(Unit()) } {
  return Bool:False(Unit())
}
fblc-test $prg main { Bool:True(Unit()) Bool:False(Unit()) } {
  return Bool:True(Unit())
}
