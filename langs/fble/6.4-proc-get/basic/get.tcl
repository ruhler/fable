fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # A basic proc get.
  Bool@ ~ get, put;
  Unit@ _ := put(true);
  Bool@ result := get();
  $(result.true);
}
