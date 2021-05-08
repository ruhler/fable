fble-test-runtime-error 6:11 {
  # Test that fble-test fails when it executes the evaluated process.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());
  !(false.true);
}
