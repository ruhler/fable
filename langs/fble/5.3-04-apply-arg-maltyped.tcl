fble-test-error 12:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  \(Bool@, Bool@; Bool@) Snd = \(Bool@ x, Bool@ y) {
    y;
  };

  # The second argument to the function fails to compile.
  Snd[false][zzz];
}
