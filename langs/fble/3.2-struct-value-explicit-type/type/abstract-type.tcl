fble-test-compile-error 7:5 {
  @ Unit@ = *();

  <@ A@> {
    # There's no way to know if A@ is a struct type, so this should not be
    # allowed.
    A@(Unit@());
  };
}
