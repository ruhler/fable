fble-test-error 6:5 {
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);

  # The open paren is missing.
  + S1@ a, Unit@ b);
}
