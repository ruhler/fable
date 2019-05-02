fble-test-error 11:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ z = Pair@(true, false);

  # The field name is missing.
  z.;
}
