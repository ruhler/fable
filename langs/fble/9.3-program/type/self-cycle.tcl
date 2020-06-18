fble-test-error 3:16 {
  % True = /Unit%.True;

  True.true;
} {
  Unit {
    # A module may not depend on itself.
    @ Unit@ = /Unit%.Unit@;

    @(Unit@);
  } {}
}
