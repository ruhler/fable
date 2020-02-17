fble-test-error 3:9 {
  # Relative references are not allowed.
  { Unit%; @(Unit); };

  Unit;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();
    @(Unit@, Unit);
  } {}
}
