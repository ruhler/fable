set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main(Bool <~ get ; ; Bool) {
    # The get port has already been defined.
    Bool <~> get, put;
    Bool putted = ~put(Bool:true(Unit()));
    ~get();
  };
}
fblc-check-error $prg 7:14
