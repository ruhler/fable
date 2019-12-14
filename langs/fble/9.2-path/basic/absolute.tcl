fble-test {
  # Use an absolute path for importing.
  { /Unit%; @(Unit); };

  Unit;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();
    @(Unit@, Unit);
  } {}
}
