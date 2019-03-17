fble-test-error 6:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  !Unit@ mkUnit = $(Unit@());
  !Bool@ mkBool = mkUnit;
  mkBool;
}
