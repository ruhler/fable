fble-test-error 3:17 {
  # The module path is not well formed.
  % Unit = /Unit.Unit;

  Unit;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();
    @(Unit@, Unit);
  }
}
