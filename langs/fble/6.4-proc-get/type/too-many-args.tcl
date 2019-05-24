fble-test-error 10:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Bool@ ~ get, put;
  Bool@ ignored := put(true);

  # There are too many arguments to the get.
  Bool@ result := get(true);
  $(result.true);
}
