
fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Char@ = +(Unit@ a, Unit@ b, Unit@ c, Unit@ d, Unit@ e);

  # Basic test of a conditional expression with a default branch.
  # Selects the default choice.
  Char@(b: Unit@()).?(
    a: true,
    d: true,
     : false
  ).false;
}
