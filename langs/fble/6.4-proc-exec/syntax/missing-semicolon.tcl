fble-test-compile-error 9:1 {
  @ Unit@ = *();
  @ Pair@ = *(Unit@ a, Unit@ b);

  # The exec statement is missing the semicolon.
  Unit@ x := !(Unit@()), Unit@ y := !(Unit@())
  !(Pair@(x, y));
}
