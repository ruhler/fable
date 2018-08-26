fble-test-error 5:30 {
  # The return type does not compile
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ T@ = \(Bool@ x, Bool@ y; zzz@);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
