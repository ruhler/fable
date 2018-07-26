fble-test-error 8:33 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool t = Bool@(true: Unit@());
  Bool f = Bool@(false: Unit@());

  # The second argument fails to compile.
  Bool z = ?(f; true: f, false: zzz);
  z.true;
}
