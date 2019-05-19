fble-test-error 5:4 {
  # The argument type is not a type.
  @ Unit@ = *();
  Unit@ u = Unit@();
  (u) { Unit@; };
}
