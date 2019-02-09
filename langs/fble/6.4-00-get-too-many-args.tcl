fble-test-error 10:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Bool@ ~ get, put;
  Bool@ ignored := put(true);

  # Too many args to get
  Bool@ result := get(false);
  $(result.true);
}
