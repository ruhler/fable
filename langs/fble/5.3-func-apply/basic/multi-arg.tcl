fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  (Bool@, Bool@) { Bool@; } Snd = (Bool@ a, Bool@ b) {
    b;
  };

  # Function application of multiple arguments.
  Unit@ _w = Snd(true, true).true;
  Unit@ _x = Snd(true, false).false;
  Unit@ _y = Snd(false, true).true;
  Unit@ _z = Snd(false, false).false;
  Unit@();
}
