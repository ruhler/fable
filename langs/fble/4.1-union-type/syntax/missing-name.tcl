fble-test-compile-error 6:9 {
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);

  # The name of a field is missing.
  +(S1@ , Unit@ b);
}
