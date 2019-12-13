fble-test-error 6:3 {
  @ Unit@ = *();

  # The semicolon after the declaration is missing.
  Unit@ x = Unit@()
  Unit@();
}
