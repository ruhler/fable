set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; $(Bool:false(Unit())), $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    // The argument is malformed.
    sub( ; ???);
  };
}

expect_malformed $prg main

