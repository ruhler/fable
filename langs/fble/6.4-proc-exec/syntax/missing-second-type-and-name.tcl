fble-test-compile-error 6:26 {
  @ Unit@ = *();
  @ Pair@ = *(Unit@ a, Unit@ b);

  # The second action of the exec is missing the type and name.
  Unit@ x := !(Unit@()), := !(Unit@);
  !(Pair@(x, y));
}
