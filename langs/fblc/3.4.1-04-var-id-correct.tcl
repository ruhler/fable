set prg {
  # Verify that a variable refers to the proper value.
  struct Unit();
  union Bool(Unit True, Unit False);

  func main(Bool x, Bool y; Bool) {
    y;
  };
}

fblc-test $prg main { Bool:False(Unit()) Bool:True(Unit()) } {
  return Bool:True(Unit())
}
fblc-test $prg main { Bool:True(Unit()) Bool:False(Unit()) } {
  return Bool:False(Unit())
}
