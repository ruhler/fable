fble-test-error 9:21 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Verify that arguments are evaluated when the struct is evaluated.
  @ Pair@ = *(Bool@ x, Unit@ y);
  Pair@(true, false.true);
}
