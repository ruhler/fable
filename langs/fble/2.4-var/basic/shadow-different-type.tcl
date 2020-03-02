fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # It is legal to shadow variables that are in scope, including when they
  # have different types.
  Unit@ a = Unit@();
  {
    Bool@ a = Bool@(false: Unit@());
    a;
  }.false;
}
