fble-test-error 4:20 {
  # The fields of a struct type must have different names.
  @ Unit@ = *();
  *(Unit@ x, Unit@ x);
}
