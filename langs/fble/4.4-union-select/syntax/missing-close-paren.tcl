fble-test-error 8:26 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The close paren is missing.
  ?(t; true: f, false: t ;
}
