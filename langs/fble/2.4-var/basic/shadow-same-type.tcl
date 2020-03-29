fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # It is legal to shadow variables that are in scope, including when they
  # have the same type.
  Bool@ a = Bool@(true: Unit@());
  Unit@ _ = {
    Bool@ a = Bool@(false: Unit@());
    a;
  }.false;
  a;
}
