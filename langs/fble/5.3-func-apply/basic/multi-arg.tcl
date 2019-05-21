fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@, Bool@) { Bool@; } Snd = (Bool@ a, Bool@ b) {
    b;
  };

  # Function application of multiple arguments.
  Unit@ w = Snd(true, true).true;
  Unit@ x = Snd(true, false).false;
  Unit@ y = Snd(false, true).true;
  Unit@ z = Snd(false, false).false;
  Unit@();
}
