fble-test-error 5:9 {
  @ Unit@ = *();

  # The value x must not be vacuously defined like this.
  Unit@ x = x;

  Unit@();
}
