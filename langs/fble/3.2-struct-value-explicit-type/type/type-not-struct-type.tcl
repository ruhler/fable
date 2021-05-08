fble-test-compile-error 6:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The struct value type is not a struct type.
  Bool@(Unit@(), Unit@());
}
