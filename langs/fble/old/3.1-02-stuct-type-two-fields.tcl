fble-test {
  # A struct type that has two fields.
  @ Unit@ = *();
  @ T@ = *(Unit@ x, Unit@ y);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
