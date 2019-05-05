fble-test-error 8:6 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The semicolon has been replaced by a comma.
  ?(t, true: f, false: t);
}
