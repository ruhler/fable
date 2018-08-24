fble-test-error 9:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The union value type fails to compile.
  @ Maybe@ = +(Bool@ just, Unit@ nothing);
  zzz@(just: true);
}
