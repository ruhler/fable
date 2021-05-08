fble-test-runtime-error 10:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  @ Pair@ = *(Bool@ x, Bool@ y);
  @ Maybe@ = +(Unit@ nothing, Pair@ just);
  Maybe@ m = Maybe@(nothing: Unit@());

  # The object is evaluated strictly before the field access.
  m.just.x;
}
