fble-test {
  # A function type that has no arguments.
  @ Unit@ = *();
  @ T@ = \( ; Unit@);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
