fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Construct a basic struct value with one argument.
  @ BoolSingle@ = *(Bool@ x);
  BoolSingle@(true);
}
