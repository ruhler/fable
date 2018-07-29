fble-test-error 8:3 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());
  Bool false = Bool@(false: Unit@());

  # The function zzz fails to compile.
  zzz(false, true);
}
