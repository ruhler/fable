fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit@());

  # Take typeof a value.
  @<True> False = Bool@(false: Unit@());
  False.false;
}
