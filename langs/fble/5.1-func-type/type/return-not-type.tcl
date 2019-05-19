fble-test-error 5:13 {
  # The return type is not a type.
  @ Unit@ = *();
  Unit@ u = Unit@();
  (Unit@) { u; };
}
