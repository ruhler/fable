fble-test {
  # Test that a variable can be used in multiple places to share a value.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ a, Bool@ b);

  Bool@ x = Bool@(true: Unit@());
  Pair@ p = Pair@(x, x);
  Unit@ _ = p.a.true;
  Unit@ _ = p.b.true;
  Unit@();
}
