fble-test-compile-error 9:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A and B are different types because the field name differs.
  @ A@ = +(Unit@ x, Bool@ y);
  @ B@ = +(Unit@ x, Bool@ z);
  A@ x = B@(x: Unit@());
  x;
}
