
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    // The '>' is missing.
    Bool <~ get, put;
    Bool putted = put~(Bool:true(Unit()));
    get~();
  };
}

expect_malformed $prg main
expect_malformed_b $prg 2

