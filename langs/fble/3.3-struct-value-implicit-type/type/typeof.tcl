fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  # Test that the type of an implicit type struct value matches what we
  # expect.
  @ S@ = *(Bool@ x, Maybe@ y);
  S@ s = @(x: true, y: nothing);
  s;
}
