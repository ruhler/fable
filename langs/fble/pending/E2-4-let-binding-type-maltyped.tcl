fble-test-error 6:3 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The binding type does not compile.
  zzz x = Bool@(true: Unit@());
  x;
}
