fble-test {
  { Unit%; @(Unit@, Unit); };

  Unit;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @(Unit@, Unit);
  } {}
} {
  Bool {
    # The value of the Bool module does not compile, but that shouldn't matter
    # because the main program does not depend on the module Bool.
    zzz;
  } {}
}
