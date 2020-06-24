fble-test-error 10:18 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);

  # Test that we can properly unwind a stack involving a recursive let.
  # For code coverage.
  Unit@ e = True.false;
  
  (Unit@) { Unit@; } F = (Unit@ x) {
    F(x);
  };

  e;
}
