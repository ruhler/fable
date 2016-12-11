
# Test a basic process call.

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; ; Bool) {
    $(Bool:true(Unit()));
  };

  proc main( ; ; Bool) {
    sub( ; );
  };
}

expect_result Bool:true(Unit()) $prg main
skip expect_result_b "0" $prg 3

