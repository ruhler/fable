fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A basic proc put.
  Bool@ ~ get, put;
  Bool@ a := put(true);
  Bool@ b := get();
  Unit@ at = a.true;
  Unit@ bt = b.true;
  $(Unit@());
}
