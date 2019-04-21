fble-test-error 4:21 {
  # The type for the second field of the struct does not compile.
  @ Unit@ = *();
  @ T@ = *(Unit@ x, zzz@ y);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
