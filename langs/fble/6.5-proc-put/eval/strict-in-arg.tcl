fble-test-error 10:11 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@ m = Maybe@(nothing: Unit@());

  # Put is strict in the argument.
  Bool@ ~ get, put;
  $(put(m.just));
}
