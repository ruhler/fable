fble-test-error 6:3 {
  @ Unit@ = *();

  # The body of the let is malformed.
  Unit@ x = Unit@();
  ???;
}
