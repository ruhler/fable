fble-test-error 5:5  {
  # The module Unit% is not found because it is a child of a child module.
  # It's not in the search path for this module which starts at this module's
  # immediate children.
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
