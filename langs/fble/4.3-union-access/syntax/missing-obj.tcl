fble-test-compile-error 11:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@ x = Maybe@(just: true);

  # The object is missing.
  .just;
}
