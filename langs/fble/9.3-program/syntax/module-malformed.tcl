fble-test-error 3:6 {
  @ Bool@ = /Bool%.Bool@;
  % True = /Bool%.True;
  % Flase = /Bool%.Flase;

  True.true;
} {
  Bool {
    # The value of the Bool module does not parse.
    ???;
  } {}
}
