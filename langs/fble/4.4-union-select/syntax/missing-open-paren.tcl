fble-test-compile-error 8:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The open paren is missing.
  t.? true: f, false: t);
}
