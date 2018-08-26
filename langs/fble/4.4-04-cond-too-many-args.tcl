fble-test-error 8:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # There are too many arguments to the conditional.
  Bool@ z = ?(f; true: f, false: t, blah: t);
  z.true;
}
