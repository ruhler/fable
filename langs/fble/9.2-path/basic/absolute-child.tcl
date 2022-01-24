fble-test {
  # Import from a child module using an absolute reference.
  % Unit = /Bool/Unit%.Unit;

  Unit;
} {
  Bool {
    @();
  } {
    Unit {
      @ Unit@ = *();
      Unit@ Unit = Unit@();
      @(Unit@, Unit);
    }
  }
}
