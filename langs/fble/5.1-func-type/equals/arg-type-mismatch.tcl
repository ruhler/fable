fble-test-compile-error 11:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A and B are different, because the argument type differs.
  @ A@ = (Unit@) { Bool@; };
  A@ a = (Unit@ y) { true; };

  @ B@ = (Bool@) { Bool@; };
  B@ b = a;
  b;
}
