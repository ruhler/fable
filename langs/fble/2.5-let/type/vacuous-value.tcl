fble-test-error 6:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The value x must not be vacuously defined like this.
  Bool@ x = x;

  # TODO: Change this to return the following line and skip the test given
  # that we know it fails?
  # x.true;
  Unit@();
}
