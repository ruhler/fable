fble-test-compile-error 11:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A and B are different types because the number of args is different.
  @ A@ = (Unit@, Bool@) { Bool@; };
  A@ a = (Unit@ x, Bool@ y) { true; };

  @ B@ = (Unit@, Bool@, Unit@) { Bool@; };
  B@ b = a;
  b;
}
