


# Test exec with multiple processes.

set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  struct Pair(Bool a, Bool b);

  proc main( ; ; Pair) {
    Bool x = $(Bool:true(Unit())), Bool y = $(Bool:false(Unit()));
    $(Pair(x, y));
  };
}

expect_result Pair(Bool:true(Unit()),Bool:false(Unit())) $prg main
expect_result_b "01" $prg 3

