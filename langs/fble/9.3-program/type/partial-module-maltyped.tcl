fble-test-error 7:5 {
  { Bool%; @(Bool@, True, False); };

  True.true;
} {
  Unit {
    @ Unit@ = *();
    @(Unit@);
  } {}
} {
  Bool {
    { Unit%; @(Unit@); };

    # The value of the Bool module does not compile. The value of the Unit
    # module does. This tests cleanup of loaded modules when a maltyped module
    # is encountered.
    zzz;
  } {}
}
