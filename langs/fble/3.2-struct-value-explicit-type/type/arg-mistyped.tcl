fble-test-compile-error 9:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The struct value's second arg has the wrong type.
  @ BoolPair@ = *(Bool@ x, Bool@ y);
  BoolPair@(true, Unit@());
}
