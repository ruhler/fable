fble-test-compile-error 5:20 {
  @ Unit@ = *();

  # A field called y@ should take on values of a type, not normal values.
  +(Unit@ x, Unit@ y@);
}
