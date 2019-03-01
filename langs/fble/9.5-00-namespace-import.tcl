fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ foo = Bool@(true: Unit@());
  @(true: Bool@(true: Unit@()), false: Bool@(false: Unit@()));

  Unit@ tt = foo.true;
  Unit@ tt2 = true.true;
  Unit@ ff = false.false;
  Unit@();
}
