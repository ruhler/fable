set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; true: $(Bool:false(Unit())), false: $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    # The argument is malformed.
    sub( ; ???);
  };
}
fblc-check-error $prg 11:13
