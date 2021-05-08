fble-test-runtime-error 14:17 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  
  (Unit@) { Unit@; } F = (Unit@ x) {
    x;
  };

  (Bool@) { Unit@; } G = (Bool@ x) {
    # Test that we can properly unwind a stack involving a normal call.
    Unit@ f = x.false;
    f;
  };

  # For code coverage, we want to exercise the path where G is not at the
  # bottom of the stack.
  Unit@ u = G(True);
  u;
}
