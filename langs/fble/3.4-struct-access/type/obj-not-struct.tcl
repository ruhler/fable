fble-test-compile-error 6:3 {
  @ Unit@ = *();
  (Unit@){Unit@;} mkUnit = (Unit@ x) { x; };

  # The object of the access is not a struct value.
  mkUnit.just;
}
