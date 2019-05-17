fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ s = Pair@(Bool@(true: Unit@()), Bool@(false: Unit@()));
  Unit@ u = Unit@();

  # The variable u should be visible in scope.
  s; u;
}
