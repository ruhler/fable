
# Test that a variable can be used in multiple places to share a value.

set prg {
  struct Unit();
  union Bool(Unit True, Unit False);
  struct Pair(Bool a, Bool b);

  func main(Bool x ; Pair) {
    Pair(x, x);
  };
}

expect_result Pair(Bool:True(Unit()),Bool:True(Unit())) $prg main Bool:True(Unit())

