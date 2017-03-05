set prg {
  # Test that a variable can be used in multiple places to share a value.
  struct Unit();
  union Bool(Unit True, Unit False);
  struct Pair(Bool a, Bool b);

  func main(Bool x ; Pair) {
    Pair(x, x);
  };
}

fblc-test $prg main { Bool:True(Unit()) } {
  return Pair(Bool:True(Unit()),Bool:True(Unit()))
}
