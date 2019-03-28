fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Construct an anonymous struct value with types using implicit field values.
  Bool@ x = true;
  Bool@ y = false;

  @ BoolPair@ = *(@<Bool@> Bool@, Bool@ x, Bool@ y);
  BoolPair@ value = @(Bool@, x, y);

  Unit@ tt = value.x.true;
  Unit@ ff = value.y.false;
  value;
}
