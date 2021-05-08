fble-test-compile-error 5:9 {
  @ Unit@ = *();

  # The variable name is missing.
  Unit@ = Unit@();
  Unit@();
}
