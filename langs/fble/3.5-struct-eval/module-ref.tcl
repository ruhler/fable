fble-test {
  # Test that modules can be accessed from within a struct eval.
  @() {
    Bool% { @(Bool@, True, False); };

    True.true;
  };
} {
  Bool {
    @ Unit@ = *();
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit@());
    Bool@ False = Bool@(false: Unit@());
    @(Unit@, Bool@, True, False);
  } {}
}
