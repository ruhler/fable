fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ z = Pair@(true, false);

  # The type of an access expression is the type of the field being accessed.
  Bool@ result = z.x;
  result;
}
