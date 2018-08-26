fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  \(Bool@ x, Bool@ y; Bool@) Snd = \(Bool@ x, Bool@ y) {
    y;
  };

  Snd(false, true).true;
}
