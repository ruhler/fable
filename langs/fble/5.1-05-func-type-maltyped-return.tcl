fble-test-error 5:25 {
  # The return type does not compile
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ T@ = [Bool@][Bool@]{zzz@;};
  @ MaybeT@ = +(Unit@ nothing, T@ just);
  MaybeT@(nothing: Unit@());
}
