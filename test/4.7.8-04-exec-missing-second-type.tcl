set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  struct Pair(Bool a, Bool b);

  proc main( ; ; Pair) {
    // The second action of the exec is missing the type.
    Bool x = $(Bool:true(Unit())), y = $(Bool:false(Unit()));
    $(Pair(x, y));
  };
}

expect_malformed $prg main

