fble-test-error 8:38 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  # Let expression with a runtime error in the body. This is to ensure the
  # memory for the let variables is properly cleaned up.
  Unit@ x = Unit@(), Unit@ y = false.true, Unit@ z = Unit@();
  z;
}
