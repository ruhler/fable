fble-test {
  # A union type that has multiple fields.
  @ Unit@ = *();
  @ T@ = +(Unit@ x, Unit@ y);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
