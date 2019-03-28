fble-test {
  @ Unit@ = *();
  @ B@ = +(Unit@ true, Unit@ false);
  @ BoolM@ = *(@<B@> Bool@, B@ True, B@ False);
  BoolM@ BoolM = BoolM@(B@(true: Unit@()), B@(false: Unit@()));

  # Access a type field.
  BoolM.Bool@ true = BoolM.True;
  true.true;
}
