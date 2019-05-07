fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Two union types with the same fields match.
  @ A@ = +(Unit@ x, Bool@ y);
  @ B@ = +(Unit@ x, Bool@ y);
  A@ x = B@(x: Unit@());
  x;
}
