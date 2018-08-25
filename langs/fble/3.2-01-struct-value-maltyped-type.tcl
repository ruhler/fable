fble-test-error 9:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The struct value type fails to compile.
  @ BoolPair@ = *(Bool@ x, Bool@ y);
  zzz@(true, false);
}
