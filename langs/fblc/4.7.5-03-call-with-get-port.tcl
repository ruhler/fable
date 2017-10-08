set prg {
  # Test calling a process with a get port.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool- get ; ; Bool) {
    -get();
  };

  proc main( ; ; Bool) {
    Bool +- put, myget;
    Bool putted = +put(Bool:true(Unit()));
    sub(myget ; );
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
