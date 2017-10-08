set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    # The name of the get port is missing.
    Bool +- put, ;
    Bool putted = +put(Bool:true(Unit()));
    -get();
  };
}
fblc-check-error $prg 7:18
