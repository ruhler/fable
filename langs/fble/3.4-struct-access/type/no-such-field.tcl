fble-test-compile-error 10:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The field being accessed is not valid.
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ z = Pair@(true, false);
  z.w;
}
