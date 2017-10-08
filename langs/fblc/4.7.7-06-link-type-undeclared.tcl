set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    # The link type is not declared.
    Bobble +- put, get;
    Bool putted = +put(Bool:true(Unit()));
    -get();
  };
}
fblc-check-error $prg 7:5
