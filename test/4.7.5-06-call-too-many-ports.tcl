
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool <~ get ; ; Bool) {
    get~();
  };

  proc main( ; ; Bool) {
    Bool <~> myget, put;
    Bool putted = put~(Bool:true(Unit()));

    // sub takes 1 port argument, but two are provided.
    sub(myget, put ; );
  };
}

fblc-check-error $prg

