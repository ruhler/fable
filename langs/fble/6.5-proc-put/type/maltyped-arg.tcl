fble-test-error 9:7 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Bool@ ~ get, put;

  # The argument to put is not valid.
  put(zzz);
}
