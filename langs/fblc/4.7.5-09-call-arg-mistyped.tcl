set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; true: $(Bool:false(Unit())), false: $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    # Calling with a Unit when a Bool is expected.
    sub( ; Unit());
  };
}
fblc-check-error $prg 11:12
