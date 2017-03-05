set prg {
  # Test a basic process call.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; ; Bool) {
    $(Bool:true(Unit()));
  };

  proc main( ; ; Bool) {
    sub( ; );
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
