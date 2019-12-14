fble-test {
  # Import from a child module using a relative reference.
  { Bool/Unit%; @(Unit); };

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
