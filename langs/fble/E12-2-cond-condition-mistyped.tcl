fble-test-error 8:14 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool t = Bool@(true: Unit@());
  Bool f = Bool@(false: Unit@());

  # The condition isn't a union type
  Bool z = ?(Unit@(); true: f, false: t);
  z.true;
}
