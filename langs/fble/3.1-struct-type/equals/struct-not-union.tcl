fble-test-error 9:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Struct and union types are different.
  @ A@ = +(Unit@ x, Bool@ y);
  @ B@ = *(Unit@ x, Bool@ y);
  A@ x = B@(Unit@(), true);
  x;
}
