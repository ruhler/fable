fble-test-compile-error 8:7 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The fields are in the wrong order.
  f.?(false: t, true: f);
}
