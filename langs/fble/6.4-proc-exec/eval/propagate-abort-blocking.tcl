fble-test-error 14:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  @ Units@ = *(Unit@ a, Unit@ b);

  # Test that abort failures are propagated from child processes when the
  # other child processes are blocked. This is a regression test that used to
  # trigger an assertion that we attempted to run an aborted thread.
  Unit@ ~ get, put;
  Unit@ a := get, Unit@ b := {
    Bool@ f := $(false);
    $(f.true);
  };
  $(Units@(a, b));
}
