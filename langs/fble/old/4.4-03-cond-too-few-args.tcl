fble-test-error 8:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # There are too few arguments to the conditional.
  Bool@ z = ?(f; true: f);
  z.true;
}
