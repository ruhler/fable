fble-test-compile-error 5:19 {
  @ Unit@ = *();

  # multi-arg functions must have unique argument names.
  (Unit@ x, Unit@ x) { x; };
}

