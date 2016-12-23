
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    // The comma separating get and put is missing.
    Bool <~> get put;
    Bool putted = put~(Bool:true(Unit()));
    get~();
  };
}

fblc-check-error $prg

