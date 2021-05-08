fble-test-compile-error 9:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # All but the default branch fails to compile.
  # This test added to increase code coverage.
  f.?(true: zzz, : t);
}
