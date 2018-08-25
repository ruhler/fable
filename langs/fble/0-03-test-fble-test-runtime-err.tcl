fble-test-error 6:9  {
  # Test 'fble-test-error' catches runtime errors
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());
  false.true;
}
