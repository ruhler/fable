fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Test use of a normal kind spec.
  # The type of x will be automatically inferred.
  % x = Bool@(false: Unit@());

  Bool@ y = x;
  y;
}
