fble-test {
  @ Unit@ = *();
  @ B@ = +(Unit@ true, Unit@ false);

  # A struct type with a type field.
  @ BoolM@ = *(@ Bool@ = B@, B@ True, B@ False);
  BoolM@ BoolM = BoolM@(B@(true: Unit@()), B@(false: Unit@()));

  BoolM.True.true;
}
