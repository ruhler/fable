fble-test-error 7:3 {
  @ Unit@ = *();
  @ Pair@ = *(Unit@ a, Unit@ b);

  # The exec statement is missing the semicolon.
  Unit@ x := $(Unit@()), Unit@ y := $(Unit@())
  $(Pair@(x, y));
}
