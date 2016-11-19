set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; ; Bool) {
    $(Bool:true(Unit()));
  };

  proc main( ; ; Bool) {
    // The first port argument has invalid syntax.
    sub( ??? ; );
  };
}

expect_malformed $prg main
