set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  struct Pair(Bool a, Bool b);

  proc main( ; ; Pair) {
    // The exec statement is missing the semicolon.
    Bool x = $(Bool:true(Unit())), Unit y = $(Bool:false(Unit()))
    $(Pair(x, y));
  };
}

expect_malformed $prg main

