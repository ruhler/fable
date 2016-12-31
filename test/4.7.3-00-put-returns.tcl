
# Test that the put process returns the put value.
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool <~> get, put;
    ~put(Bool:true(Unit()));
  };
}

expect_result Bool:true(Unit()) $prg main
expect_result_b 0 $prg 2

