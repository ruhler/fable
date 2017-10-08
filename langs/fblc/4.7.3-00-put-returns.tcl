set prg {
  # Test that the put process returns the put value.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool +- put, get;
    +put(Bool:true(Unit()));
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
