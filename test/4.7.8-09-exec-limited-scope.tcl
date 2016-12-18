
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

expect_malformed $prg main
expect_malformed_b $prg 2

