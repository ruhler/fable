fble-test-error 8:8 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The fields are in the wrong order.
  ?(f; false: t, true: f);
}
