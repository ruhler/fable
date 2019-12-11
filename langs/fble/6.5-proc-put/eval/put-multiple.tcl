fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Test that you can put to the port twice before taking anything.
  Bool@ ~ get, put;
  Unit@ p1 := put(false);
  Unit@ p2 := put(true);
  Bool@ g1 := get;
  Bool@ g2 := get;
  $(g2.true);
}
