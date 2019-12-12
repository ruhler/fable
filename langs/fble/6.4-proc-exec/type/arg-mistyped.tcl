fble-test-error 7:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The argument to an exec has the wrong type.
  Bool@ x := $(Unit@());
  $(x);
}
