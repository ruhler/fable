fble-test-compile-error 3:15 {
  % True = /Unit%.True;

  True.true;
} {
  Unit {
    # A module may not depend on itself.
    @ Unit@ = /Unit%.Unit@;

    @(Unit@);
  } {}
}
