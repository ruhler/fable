fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A basic proc put.
  Bool@ ~ get, put;
  Unit@ a := put(true);
  Bool@ b := get;
  Unit@ bt = b.true;
  $(bt);
}
