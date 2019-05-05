fble-test-error 9:12 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # You cannot use implicit field names for union select expressions like you
  # can with implicit type struct value expressions.
  ?(true; true, false);
}
