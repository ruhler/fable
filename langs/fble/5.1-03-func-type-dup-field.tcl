fble-test-error 5:27 {
  # The arguments to a function must have different names. 
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ T@ = \(Bool@ x, Bool@ x; Bool@);
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
