fble-test-error 6:28 {
  @ Unit@ = *();
  @ Pair@ = *(Unit@ a, Unit@ b);

  # The second action of the exec is missing the type.
  Unit@ x := !(Unit@()), y := !(Unit@);
  !(Pair@(x, y));
}
