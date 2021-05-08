fble-test-compile-error 5:17 {
  @ Unit@ = *();

  # You cannot have a field that is a type of a type.
  *(@<@<Unit@>> x@);
}
