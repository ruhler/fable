fble-test-error 10:21 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);

  # Test that we can properly unwind a stack involving a link process.
  # For code coverage.
  Unit@ e := !(True.false);
  Unit@ ~ g, p;
  p(e);
}
