

# Test a simple exec process.

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool x = $(Bool:true(Unit()));
    $(x);
  };
}

expect_result Bool:true(Unit()) $prg main
