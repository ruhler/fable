set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub(Bool <~ get ; ; Bool) {
    ~get();
  };

  proc main( ; ; Bool) {
    Bool <~> myget, put;
    Bool putted = ~put(Bool:true(Unit()));
    # The semicolon after the port arguments is missing.
    sub(myget);
  };
}
fblc-check-error $prg 13:14
