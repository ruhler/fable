fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  Bool@ foo = Bool@(true: Unit@());

  Bool@ bar = @(true: Bool@(true: Unit@()), false: Bool@(false: Unit@())) {
    Bool@ foo = false;
    foo;
  };

  Unit@ tt = foo.true;
  Unit@ ff = bar.false;
  Unit@();
}
