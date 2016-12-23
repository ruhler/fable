set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; $(Bool:false(Unit())), $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    // Calling with a Unit when a Bool is expected.
    sub( ; Unit());
  };
}

fblc-check-error $prg

