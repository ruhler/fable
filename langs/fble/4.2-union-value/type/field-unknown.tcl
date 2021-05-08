fble-test-compile-error 9:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The union value sets an invalid field.
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@(crazy: true);
}
