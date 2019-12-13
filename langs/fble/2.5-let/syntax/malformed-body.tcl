fble-test-error 6:4 {
  @ Unit@ = *();

  # The body of the let is malformed.
  Unit@ x = Unit@();
  ???;
}
