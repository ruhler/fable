fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Construct a basic struct value.
  @ BoolPair@ = *(Bool@ x, Bool@ y);
  BoolPair@(true, false);
}
