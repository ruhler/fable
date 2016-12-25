
# Test a simple link process.

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool <~> get, put;
    Bool putted = put~(Bool:true(Unit()));
    get~();
  };
}

expect_result Bool:true(Unit()) $prg main
expect_result_b 0 $prg 2

