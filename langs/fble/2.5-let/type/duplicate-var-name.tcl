fble-test-compile-error 7:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Duplicate variable names are not allowed in a let expression.
  Bool@ x = Bool@(true: Unit@()),
  Bool@ x = Bool@(false: Unit@());
  x.true;
}
