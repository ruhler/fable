fble-test-compile-error 6:3 {
  @ Unit@ = *();

  # The semicolon is missing at the end of the link statement.
  Unit@ ~ get, put
  get;
}
