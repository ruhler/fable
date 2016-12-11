

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    // The variable y is not declared.
    Bool x = $(y);
    $(x);
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

