fble-test-compile-error 6:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The binding type is not a type.
  Unit@() x = Unit@();
  x;
}
