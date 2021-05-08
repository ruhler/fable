fble-test-compile-error 9:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Bool@ just, Unit@ nothing);

  # The argument value is missing.
  Maybe@(just: );
}
