fble-test-runtime-error 8:25 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  # Verify that the argument is evaluated when the union is evaluated.
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@(nothing: false.true);
}
