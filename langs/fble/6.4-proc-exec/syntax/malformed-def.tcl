fble-test-error 6:38 {
  @ Unit@ = *();
  @ Pair@ = *(Unit@ a, Unit@ b);

  # The second action of the exec has a poorly formed action.
  Unit@ x := $(Unit@()), Unit@ y := ???;
  $(Pair@(x, y));
}
