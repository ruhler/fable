fble-test-compile-error 7:5 {
  % True = /Bool%.True;

  True.true;
} {
  Unit {
    @ Unit@ = *();
    @(Unit@);
  } {}
} {
  Bool {
    @ Unit@ = /Unit%.Unit@;

    # The value of the Bool module does not compile. The value of the Unit
    # module does. This tests cleanup of loaded modules when a maltyped module
    # is encountered.
    zzz;
  } {}
}
