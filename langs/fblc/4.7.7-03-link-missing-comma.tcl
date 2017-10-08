set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    # The comma separating get and put is missing.
    Bool +- put get;
    Bool putted = +put(Bool:true(Unit()));
    -get();
  };
}
fblc-check-error $prg 7:17
