fble-test-error 4:21 {
  # The fields of an inline union type must have different names.
  @ Unit@ = *$();
  +$(Unit@ x, Unit@ x);
}
