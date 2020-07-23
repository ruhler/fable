fble-test {
  @ Unit@ = *$();
  @ Bool@ = +$(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Basic inline struct access.
  @ Pair@ = *$(Bool@ x, Bool@ y);
  Pair@ z = Pair@(true, false);
  $(z.y).false;
}
