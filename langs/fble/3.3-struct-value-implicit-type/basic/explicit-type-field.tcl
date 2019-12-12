fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Construct an anonymous struct value with type fields.
  @ BoolPair@ = *(@<Bool@> B@, Bool@ x, Bool@ y);
  BoolPair@ value = @(B@: Bool@, x: true, y: false);

  Unit@ tt = value.x.true;
  Unit@ ff = value.y.false;
  value;
}
