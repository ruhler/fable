fble-test-error 8:17 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false, Unit@ other);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The One of the choices is missing.
  ?(t; true: f, , other: t);
}
