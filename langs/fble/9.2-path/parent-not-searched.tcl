fble-test-error 4:7 {
  { Bool%; @(True); };

  True;
} {
  Unit {
    @ Unit@ = *();
    Unit@ Unit = Unit@();

    @(Unit@, Unit);
  } {}
} {
  Bool {
    # Bool% cannot access Unit% using a relative path, because Unit% is not a
    # child of Bool%.
    { Unit%; @(Unit@, Unit); };
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit);
    @(Bool@, True);
  } {}
}
