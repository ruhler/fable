
fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Char@ = +(Unit@ a, Unit@ b, Unit@ c, Unit@ d, Unit@ e);

  # Basic test of a conditional expression with a default branch.
  # Selects an explicit choice.
  ?(Char@(d: Unit@());
      a: true,
      d: true,
       : false).true;
}
