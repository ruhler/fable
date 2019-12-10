fble-test-error 8:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The default argument fails to compile.
  ?(f; true: f, : zzz);
}
