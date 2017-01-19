set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; $(Bool:false(Unit())), $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    # sub takes 1 argument, not 2.
    sub( ; Bool:true(Unit()), Bool:false(Unit()));
  };
}
fblc-check-error $prg 11:5
