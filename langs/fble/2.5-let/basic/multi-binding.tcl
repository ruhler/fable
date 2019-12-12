fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ BoolPair@ = *(Bool@ a, Bool@ b);

  # Let expr with multiple bindings.
  Bool@ x = Bool@(false: Unit@()), Bool@ y = Bool@(true: Unit@());
  BoolPair@(x, y);
}
