fble-test {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # Basic struct access.
  @ BoolPair = *(Bool x, Bool y);
  BoolPair z = BoolPair@(true, false);
  z.x;
}
