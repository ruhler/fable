fble-test-error 6:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The value x must not be vacuously defined like this.
  Bool@ x = x;
  x.true;
}
