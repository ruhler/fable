fble-test-error 5:12 {
  @ Unit@ = *();

  # Foo@ should be a type given its name, but it is assigned to a value.
  @ Foo@ = Unit@();

  Unit@();
}
