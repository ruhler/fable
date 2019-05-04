fble-test-error 9:15 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # The colon is missing.
  Maybe@(just true);
}
