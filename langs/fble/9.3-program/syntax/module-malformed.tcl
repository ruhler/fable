fble-test-error 3:6 {
  { Bool%; @(Bool@, True, False); };

  True.true;
} {
  Bool {
    # The value of the Bool module does not parse.
    ???;
  } {}
}
