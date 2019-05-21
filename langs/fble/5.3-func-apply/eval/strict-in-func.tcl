fble-test-error 10:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +((Bool@){Bool@;} just, Unit@ nothing);
  Maybe@ m = Maybe@(nothing: Unit@());

  # Function application is strict in the function.
  m.just(true);
}
