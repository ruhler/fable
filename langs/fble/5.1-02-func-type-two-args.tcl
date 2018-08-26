fble-test {
  # A function type that has a two arguments
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ T@ = \(Bool@ x, Bool@ y; Bool@);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
