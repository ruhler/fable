fble-test-error 6:3 {
  @ Unit@ = *();
  \( ; Unit@) mkUnit = \() { Unit@(); };

  # The object of the access is not a struct or union value.
  mkUnit.just;
}
