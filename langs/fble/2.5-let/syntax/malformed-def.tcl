fble-test-compile-error 5:13 {
  @ Unit@ = *();

  # The variable definition is malformed.
  Unit@ x = ???;
  Unit@();
}
