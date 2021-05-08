fble-test-compile-error 8:26 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # There are too many choices to the conditional.
  f.?(true: f, false: t, blah: t);
}
