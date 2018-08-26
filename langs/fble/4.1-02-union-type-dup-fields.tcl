fble-test-error 4:27 {
  # The fields of the union must have different names.
  @ Unit@ = *();
  @ T@ = +(Unit@ x, Unit@ x);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
