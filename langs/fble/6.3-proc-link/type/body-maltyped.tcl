fble-test-error 7:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The body of the link is not well typed.
  Bool@ ~ get, put;
  zzz;
}
