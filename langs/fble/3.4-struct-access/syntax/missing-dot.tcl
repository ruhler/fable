fble-test-compile-error 11:6 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ z = Pair@(true, false);

  # The dot is missing.
  z x;
}
