fble-test-error 8:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The second arguments type fails to compile.
  [Bool@ x][zzz@ y] {
    ?(x; true: true, false: y);
  };
}
