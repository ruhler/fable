fble-test {
  # Test that a get will block for something to be put.
  # We can't guarantee this test will exercise the desired case, because it
  # depends on how things are scheduled, but hopefully it will test it.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  Bool@ ~ get, put;
  Unit@ p1 := put(true), Bool@ g1 := get;
  Bool@ g2 := get, Unit@ p2 := put(true);
  !(g2.true);
}
