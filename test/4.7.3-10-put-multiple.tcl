

# Test that you can put twice to the same port before getting.

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool <~> get, put;
    Bool p1 = put~(Bool:false(Unit()));
    Bool p2 = put~(Bool:true(Unit()));
    Bool g1 = get~();
    get~();
  };
}

expect_result Bool:true(Unit()) $prg main
skip expect_result_b "0" $prg 2

