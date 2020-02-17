fble-test-error 3:5  {
  # The module Unit% is not found because it is a child of a child module.
  { Unit%; @(Unit); };

  Unit;
} {
  Bool {
    { Unit%; @(Unit@, Unit); };
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);
    @(Bool@, True);
  } { {
      Unit {
        @ Unit@ = *();
        Unit@ Unit = Unit@();
        @(Unit@, Unit);
      } {}
    }
  }
}
