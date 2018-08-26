fble-test-error 8:18 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The tags are in the wrong order.
  Bool@ z = ?(f; false: t, true: f);
  z.true;
}
