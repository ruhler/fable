fble-test-error 12:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@) { Bool@; } Not = (Bool@ x) {
    ?(x; true: false, false: true);
  };

  # The function does not compile.
  zzz(true);
}
