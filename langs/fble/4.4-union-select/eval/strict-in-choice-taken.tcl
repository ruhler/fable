fble-test-error 8:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The taken choice is evaluated.
  ?(t; true: f.true, false: t.true);
}
