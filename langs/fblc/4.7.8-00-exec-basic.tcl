set prg {
  # Test a simple exec process.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool x = $(Bool:true(Unit()));
    $(x);
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
