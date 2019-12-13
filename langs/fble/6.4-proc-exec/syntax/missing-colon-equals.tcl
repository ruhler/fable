fble-test-error 6:34 {
  @ Unit@ = *();
  @ Pair@ = *(Unit@ a, Unit@ b);

  # The second action of the exec is missing the := sign.
  Unit@ x := $(Unit@()), Unit@ y $(Unit@);
  $(Pair@(x, y));
}
