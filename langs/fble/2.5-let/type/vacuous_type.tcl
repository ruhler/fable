fble-test-error 5:5 {
  @ Unit@ = *();

  # The type T@ must not be vacuously defined like this.
  @ T@ = T@;

  Unit@();
}
