fble-test-runtime-error 6:9  {
  # Test 'fble-test-runtime-error' catches runtime errors
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());
  false.true;
}
