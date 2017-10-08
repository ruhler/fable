set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool- get ; ; Bool) {
    -get();
  };

  proc main( ; ; Bool) {
    Bool +- put, myget;
    Bool putted = +put(Bool:true(Unit()));

    # sub takes 1 port argument, but two are provided.
    sub(myget, put ; );
  };
}
fblc-check-error $prg 14:5
