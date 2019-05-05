fble-test-error 8:7 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The semicolon is missing after the condition.
  ?(t true: f, false: t);
}
