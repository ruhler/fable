fble-test-error 5:14 {
  # The type for the second field of the struct is not a type.
  @ Unit@ = *();
  Unit@ u = Unit@();
  *(Unit@ x, u y);
}
