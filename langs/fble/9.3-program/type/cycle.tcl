fble-test-error 2:8 {
  { Unit%; @(True); };

  True.true;
} {
  Bool {
    { /Unit%; @(Unit@); };
    @ Bool@ = +(Unit@ true, Unit@ false);
    @(Bool@);
  } {}
} {
  Unit {
    # Recursive module dependencies are not allowed.
    @ Unit@ = *();

    { /Bool%; @(Bool@); };
    Bool@ True = Bool@(True: Unit@());

    @(Unit@, True);
  } {}
}
