fble-test-compile-error 10:24 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ BoolPair@ = *(Bool@ x, Bool@ y);

  # Close paren is missing.
  BoolPair@(true, false;
}
