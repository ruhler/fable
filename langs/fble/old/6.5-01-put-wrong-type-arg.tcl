fble-test-error 8:7 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Bool@ ~ get, put;
  # The type passed to put should be Bool@, not Unit@
  put(Unit@());
}
