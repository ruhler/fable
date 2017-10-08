set prg {
  # Test a simple link process.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool +- put, get;
    Bool putted = +put(Bool:true(Unit()));
    -get();
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
