fble-test-error 9:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A and B are different types because the number of fields is different.
  @ A@ = +(Unit@ x, Bool@ y);
  @ B@ = +(Unit@ x, Bool@ y, Unit@ z);
  A@ x = B@(x: Unit@());
  x;
}
