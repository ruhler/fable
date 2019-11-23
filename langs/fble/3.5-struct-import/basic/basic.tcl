fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ s = Pair@(Bool@(true: Unit@()), Bool@(false: Unit@()));

  # Test basic struct import
  s;
  Unit@ tt = x.true;
  Unit@ ff = y.false;
  Unit@();
}
