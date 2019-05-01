fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Test that the type of the struct is the type we specify for it.
  @ BoolPair@ = *(Bool@ x, Bool@ y);
  BoolPair@ x = BoolPair@(true, false);
  x;
}
