fble-test-error 8:6 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The initial ? is missing.
   (t; true: f, false: t);
}
