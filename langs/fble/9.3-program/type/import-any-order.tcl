fble-test {
  # It's perfectly valid to import the Bool module before the Unit module,
  # even though the Bool module depends on Unit.
  % True = /Bool%.True;
  @ Unit@ = /Unit%.Unit@;

  Unit@ u = True.true;
  u;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @(Unit@, Unit);
  } {}
} {
  Bool {
    @ Unit@ = /Unit%.Unit@;
    % Unit = /Unit%.Unit;
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);
    @(Bool@, True);
  } {}
}
