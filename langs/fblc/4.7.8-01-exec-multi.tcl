set prg {
  # Test exec with multiple processes.
  struct Unit();
  union Bool(Unit true, Unit false);
  struct Pair(Bool a, Bool b);

  proc main( ; ; Pair) {
    Bool x = $(Bool:true(Unit())), Bool y = $(Bool:false(Unit()));
    $(Pair(x, y));
  };
}

fblc-test $prg main {} {
  return Pair(Bool:true(Unit()),Bool:false(Unit()))
}
