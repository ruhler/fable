fble-test-error 8:26 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool t = Bool@(true: Unit@());
  Bool f = Bool@(false: Unit@());

  # The second argument has an invalid tag.
  Bool z = ?(f; true: f, blah: t);
  z.true;
}
