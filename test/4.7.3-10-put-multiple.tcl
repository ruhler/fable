set prg {
  # Test that you can put twice to the same port before getting.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool <~> get, put;
    Bool p1 = ~put(Bool:false(Unit()));
    Bool p2 = ~put(Bool:true(Unit()));
    Bool g1 = ~get();
    ~get();
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
