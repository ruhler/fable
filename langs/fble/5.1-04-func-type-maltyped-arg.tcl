fble-test-error 5:19 {
  # The type for the second arg of the function does not compile.
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ T@ = \(Bool@, zzz@; Bool@);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
