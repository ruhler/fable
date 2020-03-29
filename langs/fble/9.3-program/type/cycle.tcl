fble-test-error 2:16 {
  % True = /Unit%.True;

  True.true;
} {
  Bool {
    @ Unit@ = /Unit%.Unit@;
    @ Bool@ = +(Unit@ true, Unit@ false);
    @(Bool@);
  } {}
} {
  Unit {
    # Recursive module dependencies are not allowed.
    @ Unit@ = *();

    @ Bool@ = /Bool%.Bool@;
    Bool@ True = Bool@(True: Unit@());

    @(Unit@, True);
  } {}
}
