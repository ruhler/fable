fble-test {
  # A struct type that has a single field.
  @ Unit@ = *();
  @ T@ = *(Unit@ x);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
