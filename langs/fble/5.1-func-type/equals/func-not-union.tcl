fble-test-compile-error 9:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Function and union types are different.
  @ A@ = +(Unit@ x, Bool@ y);
  @ B@ = (Unit@) { Bool@; };
  A@ x = (Unit@ x) { true; };
  x;
}
