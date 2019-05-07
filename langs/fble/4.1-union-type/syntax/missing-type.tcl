fble-test-error 6:6 {
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);

  # The type of a field is missing.
  +(a, Unit@ b);
}
