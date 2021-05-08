fble-test-compile-error 9:20 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # There are too many arguments.
  Maybe@(just: true, nothing: Unit@());
}
