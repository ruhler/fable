fble-test-error 8:7 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The open paren is missing.
  t.? true: f, false: t);
}
