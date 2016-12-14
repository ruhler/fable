
# Test process call that takes regular arguments.
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc sub( ; Bool x; Bool) {
    ?(x ; $(Bool:false(Unit())), $(Bool:true(Unit())));
  };

  proc main( ; ; Bool) {
    sub( ; Bool:false(Unit()));
  };
}

expect_result Bool:true(Unit()) $prg main
# skip: requires support for procs
skip expect_result_b "0" $prg 3

