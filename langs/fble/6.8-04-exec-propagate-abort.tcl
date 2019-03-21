fble-test-error 10:47 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Units@ = *(Unit@ a, Unit@ b);

  # Test that abort failures are propagated from child processes.
  Unit@ a := $(true.true), Unit@ b := {
    Bool@ f := $(false);
    $(f.true);
  };
  $(Units@(a, b));
}
