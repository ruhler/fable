fble-test {
  # Use a relative path for importing.
  { Unit%; @(Unit); };

  Unit;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();
    @(Unit@, Unit);
  } {}
}
