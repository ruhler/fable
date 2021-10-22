fble-test-compile-error 8:8 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @? Tok@;

  # The abstract type is not a type.
  Tok@<Unit>;
}
