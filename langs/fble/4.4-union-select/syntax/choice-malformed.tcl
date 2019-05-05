fble-test-error 8:25 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # one of the choices has bad syntax.
  ?(t; true: f, false: ???);
}
