set prg {
  # Test calling a process with multiple ports.
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool- get, Bool+ put ; ; Bool) {
    Bool b = -get();
    +put(b);
  };

  proc main( ; ; Bool) {
    Bool +- myput, myget;
    Bool putted = +myput(Bool:true(Unit()));
    sub(myget, myput; );
  };
}

fblc-test $prg main {} "return Bool:true(Unit())"
