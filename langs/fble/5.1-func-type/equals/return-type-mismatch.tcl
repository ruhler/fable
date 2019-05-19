fble-test-error 11:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A and B are different, because the return type differs.
  @ A@ = (Unit@) { Bool@; };
  A@ a = (Unit@ y) { true; };

  @ B@ = (Unit@) { Unit@; };
  B@ b = a;
  b;
}
