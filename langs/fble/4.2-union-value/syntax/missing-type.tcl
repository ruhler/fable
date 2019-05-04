fble-test-error 9:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # The type is missing.
  (just: true);
}
