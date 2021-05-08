fble-test-compile-error 12:7 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@) { Bool@; } Not = (Bool@ x) {
    x.?(true: false, false: true);
  };

  # The argument does not compile.
  Not(zzz);
}
