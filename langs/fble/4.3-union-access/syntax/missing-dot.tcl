fble-test-error 11:9 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@ x = Maybe@(just: true);

  # The dot is missing.
  x just;
}
