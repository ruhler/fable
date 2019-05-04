fble-test-error 9:25 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Verify that the argument is evaluated when the union is evaluated.
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@(nothing: false.true);
}
