fble-test-error 10:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  Bool@ ~ get, put;

  # There are too many arguments to the put.
  put(true, false);
}
