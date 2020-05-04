fble-test-error 11:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A multi argument function type is not considered the same type as the
  # corresponding nested single argument function type.
  @ A@ = (Unit@, Bool@) { Bool@; };
  @ B@ = (Unit@) { (Bool@) { Bool@;}; };
  A@ a = (Unit@ x, Bool@ y) { true; };
  B@ b = a;
  b;
}
