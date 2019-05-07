fble-test-error 8:1 {
  @ Unit@ = *();
  @ S1@ = *(Unit@ x);

  # The file ends before the union type is completed.
  +(S1@ a,
}
