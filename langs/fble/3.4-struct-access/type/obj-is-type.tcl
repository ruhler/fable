fble-test-compile-error 5:3 {
  @ Unit@ = *();

  # The object of the access is a type, which is not allowed.
  Unit@.just;
}
