fble-test-error 9:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The union value's arg fails to compile.
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  Maybe@(just: zzz);
}
