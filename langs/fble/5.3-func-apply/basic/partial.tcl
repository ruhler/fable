fble-test-error 12:27 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@, Bool@) { Bool@; } And = (Bool@ a, Bool@ b) {
    a.?(true: b, false: false);
  };

  # Partial application is not allowed.
  (Bool@) { Bool@; } Id = And(true);

  Unit@ _tt = Id(true).true;
  Unit@ _ff = Id(false).false;
  Unit@();
}
