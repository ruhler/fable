fble-test-compile-error 5:14 {
  @ Unit@ = *();

  # The get and put ports must be named differently.
  Unit@ ~ x, x;
  x;
}
