fble-test-error 9:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A and B are different types because a field type differs.
  @ A@ = +(Unit@ x, Bool@ y);
  @ B@ = +(Unit@ x, Unit@ y);
  A@ x = B@(x: Unit@());
  x;
}
