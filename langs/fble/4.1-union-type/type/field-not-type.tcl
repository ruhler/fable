fble-test-error 5:14 {
  # The type for the second field of the union is not a type.
  @ Unit@ = *();
  Unit@ u = Unit@();
  +(Unit@ x, u x);
}
