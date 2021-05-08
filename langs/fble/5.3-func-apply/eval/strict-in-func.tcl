fble-test-runtime-error 9:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Maybe@ = +((Bool@){Bool@;} just, Unit@ nothing);
  Maybe@ m = Maybe@(nothing: Unit@());

  # Function application is strict in the function.
  m.just(true);
}
