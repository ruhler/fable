fble-test-error 12:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  \(Bool@ x, Bool@ y; Bool@) Snd = \(Bool@ x, Bool@ y) {
    y;
  };

  # The second argument to the function has the wrong type.
  Snd(false, Unit@());
}
