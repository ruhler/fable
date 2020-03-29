fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);
  Bool@ False = Bool@(false: Unit);

  # Regression test for a bug we had in the past where the values of x and y
  # got mixed up here: x was set to False and y to True by mistake.
  Bool@ x = True, Bool@ y = False;

  Unit@ _tt = x.true;
  Unit@ _ff = y.false;
  Unit;
}
