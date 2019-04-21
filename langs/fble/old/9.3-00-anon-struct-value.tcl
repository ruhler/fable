fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Construct an anonymous struct value.
  @ BoolPair@ = *(Bool@ x, Bool@ y);
  BoolPair@ value = @(x: true, y: false);

  Unit@ tt = value.x.true;
  Unit@ ff = value.y.false;
  value;
}
