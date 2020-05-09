fble-test-error 8:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # It is not legal to specify only a default value. You may as well use the
  # default value directly instead of a union select.
  true.?(: Unit@());
}
