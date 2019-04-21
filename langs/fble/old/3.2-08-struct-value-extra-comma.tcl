fble-test-error 10:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ BoolPair@ = *(Bool@ x, Bool@ y);

  # Too many commas in the argument list.
  BoolPair@(, true, false);
}
