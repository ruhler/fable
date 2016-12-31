set prg {
  struct Unit();
  union Bool(Unit true, Unit false);
  struct Pair(Bool a, Bool b);

  proc main( ; ; Pair) {
    // The second action of the exec has a poorly formed action.
    Bool x = $(Bool:true(Unit())), Unit y = ???;
    $(Pair(x, y));
  };
}
fblc-check-error $prg 8:46
