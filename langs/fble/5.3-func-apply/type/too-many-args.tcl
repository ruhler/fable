fble-test-error 12:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@, Bool@) { Bool@; } Snd = (Bool@ a, Bool@ b) {
    b;
  };

  # Two arguments are expected, but three are provided.
  Snd(true, false, true);
}
