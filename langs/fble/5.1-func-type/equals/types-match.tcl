fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Two function types with the arguments and return types match.
  @ A@ = (Unit@) { Bool@; };
  @ B@ = (Unit@) { Bool@; };
  A@ x = (Unit@ x) { true; };
  x;
}
