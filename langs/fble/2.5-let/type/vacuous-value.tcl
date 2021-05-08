fble-test-runtime-error 7:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The value x must not be vacuously defined like this.
  # TODO: Should this be a runtime error or a compilation error?
  Bool@ x = x;
  x.true;
}
