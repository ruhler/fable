fble-test-compile-error 7:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The body of the link is not a process
  Bool@ ~ get, put;
  Unit@();
}
