fble-test-runtime-error 9:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ m = Maybe@(nothing: Unit@());

  # The object is evaluated strictly before the field access.
  m.just.true;
}
