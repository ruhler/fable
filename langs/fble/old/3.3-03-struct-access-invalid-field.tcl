fble-test-error 10:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The field being accessed is not valid.
  @ BoolPair@ = *(Bool@ x, Bool@ y);
  BoolPair@ z = BoolPair@(true, false);
  z.w;
}
