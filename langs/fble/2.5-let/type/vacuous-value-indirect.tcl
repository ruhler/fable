fble-test-runtime-error 10:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  (Bool@) { Bool@; } Id = (Bool@ x) { x; };

  # The value x must not be vacuously defined like this.
  # This is a more indirect example of a vacuous value to increase code
  # coverage.
  Bool@ x = {
    Bool@ y = x;
    Id(y);
  };
  x.true;
}
