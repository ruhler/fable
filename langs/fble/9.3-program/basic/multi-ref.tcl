fble-test {
  # It's fine to refer to the same module more than once.
  /Bool%.Bool@ true = /Bool%.True;
  true.true;
} {
  Bool {
    @ Unit@ = *();
    @ Bool@ = +(Unit@ true, Unit@ false);
    Bool@ True = Bool@(true: Unit@());
    Bool@ False = Bool@(false: Unit@());
    @(Unit@, Bool@, True, False);
  }
}
