fble-test-error 9:20 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Maybe@ = +(Bool@+ just, Unit@ nothing);
  Maybe@ m = Maybe@(nothing: Unit@());

  # Put is strict in the port argument.
  Unit@! doput = m.just(true);
  Unit@();
}


