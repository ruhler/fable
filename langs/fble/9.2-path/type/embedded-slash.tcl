fble-test-error 4:12 {
  # /'Bool/Unit'% is not the same as /Bool/Unit%.
  # This should give a "no such module" error.
  % Unit = /'Bool/Unit'%.Unit;

  Unit;
} {
  Bool {
    @ Unit@ = /Unit%.Unit@;
    % Unit = /Unit%.Unit;
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);
    @(Bool@, True);
  } {
    Unit {
      @ Unit@ = *();
      Unit@ Unit = Unit@();
      @(Unit@, Unit);
    }
  }
}
