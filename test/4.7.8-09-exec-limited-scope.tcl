
# Test that expressions in exec actions do not have access to variables
# defined by other exec actions in the same parallel block.
set prg {
  struct Unit();
  union Bool(Unit true, Unit false);

  proc main( ; ; Bool) {
    Bool a = $(Bool:false(Unit())), Bool b = $(a);
    $(b);
  };
}

fblc-check-error $prg

