fble-test-error 4:21 {
  # The fields of an inline struct type must have different names.
  @ Unit@ = *$();
  *$(Unit@ x, Unit@ x);
}
