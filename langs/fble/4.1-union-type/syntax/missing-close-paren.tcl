fble-test-error 6:20 {
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);

  # The close paren is missing.
  +(S1@ a, Unit@ b ;
}
