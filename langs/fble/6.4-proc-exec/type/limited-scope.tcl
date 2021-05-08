fble-test-compile-error 6:39 {
  @ Unit@ = *();

  # Test that expressions in exec actions do not have access to variables
  # defined by other exec actions in the same parallel block.
  Unit@ x := !(Unit@()), Unit@ y := !(x);
  !(y);
}
