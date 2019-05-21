fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@, Bool@) { Bool@; } And = (Bool@ a, Bool@ b) {
    ?(a; true: b, false: false);
  };

  # Partial application.
  (Bool@) { Bool@; } Id = And(true);

  Unit@ tt = Id(true).true;
  Unit@ ff = Id(false).false;
  Unit@();
}
