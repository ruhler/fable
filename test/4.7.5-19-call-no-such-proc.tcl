set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    # sub is not defined.
    notsub( ; Bool:false(Unit()));
  };
}
fblc-check-error $prg 7:5
