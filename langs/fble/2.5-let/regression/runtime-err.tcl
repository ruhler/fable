fble-test-runtime-error 8:40 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  # Let expression with a runtime error in the body. This is to ensure the
  # memory for the let variables is properly cleaned up.
  Unit@ _x = Unit@(), Unit@ _y = false.true, Unit@ z = Unit@();
  z;
}
