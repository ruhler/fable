fble-test-error 8:17 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The second argument has an invalid field.
  ?(f; true: f, blah: t);
}
