fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # Basic test of a conditional expression.
  # Selects the first choice.
  Bool@ z = t.?(true: f, false: t);
  z.false;
}
