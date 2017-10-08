set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool- get ; ; Bool) {
    -get();
  };

  proc main( ; ; Bool) {
    Bool +- put, myget;
    Bool putted = +put(Bool:true(Unit()));
    # The second port argument has the wrong syntax. 
    sub(myget, ??? ; );
  };
}
fblc-check-error $prg 13:16
