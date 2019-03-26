fble-test-error 9:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The body of the function fails to compile.
  [Bool@ x][Bool@ y] {
    zzz;
  };
}
