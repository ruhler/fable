fble-test-error 12:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  \(Bool@, Bool@; Bool@) Snd = \(Bool@ x, Bool@ y) {
    y;
  };

  # The function is not provided with enough args.
  Snd(false);
}
