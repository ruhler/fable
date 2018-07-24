fble-test-error 9:3 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # The struct value has too many arguments.
  @ BoolPair = *(Bool x, Bool y);
  BoolPair@(true, false, false);
}
