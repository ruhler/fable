fble-test-compile-error 5:24 {
  @ Unit@ = *();

  # The let body is missing.
  { Unit@ x = Unit@(); };
}
