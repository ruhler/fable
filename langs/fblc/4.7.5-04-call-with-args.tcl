set prg {
  # Test process call that takes regular arguments.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; true: $(Bool:false(Unit())), false: $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    sub( ; Bool:false(Unit()));
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
