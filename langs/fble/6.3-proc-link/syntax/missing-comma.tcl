fble-test-compile-error 5:15 {
  @ Unit@ = *();

  # The comma separating get and put is missing.
  Unit@ ~ get put;
  get;
}
