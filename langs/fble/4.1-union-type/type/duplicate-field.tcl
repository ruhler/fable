fble-test-compile-error 4:20 {
  # The fields of a union type must have different names.
  @ Unit@ = *();
  +(Unit@ x, Unit@ x);
}
