fble-test-error 8:15 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The condition doesn't compile.
  Bool@ z = ?(zzz; true: f, false: t);
  z.true;
}
