fble-test {
  # Regression test to verify we don't leak memory when doing typeof a
  # function literal.
  @ Unit@ = *();
  @< (Unit@ x) { x; } >;
}
