fble-test-runtime-error 11:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@ m = Maybe@(nothing: Unit@());

  # The condition is evaluated first.
  m.just.?(true: f.true, false: t.false);
}
