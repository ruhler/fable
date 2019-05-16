fble-test-error 10:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ s = Pair@(Bool@(true: Unit@()), Bool@(false: Unit@()));
  Unit@ u = Unit@();

  s {
    # The variable u should not be visible in scope here.
    u;
  };
}
