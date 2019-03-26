fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Construct a basic function value.
  [Bool@ x][Bool@ y] {
    ?(x; true: true, false: y);
  };
}
