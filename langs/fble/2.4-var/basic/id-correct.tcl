fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Verify that a variable refers to the proper value.
  Bool@ x = Bool@(true: Unit@());
  Bool@ y = Bool@(false: Unit@());
  Unit@ _ = x.true;
  Unit@ _ = y.false;
  Unit@();
}
