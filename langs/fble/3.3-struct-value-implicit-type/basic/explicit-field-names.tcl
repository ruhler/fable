fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  # Implicit type struct value with explicit field names.
  @ S@ = *(Bool@ x, Maybe@ y);
  S@ s = @(x: true, y: nothing);
  Unit@ a = s.x.true;
  Unit@ b = s.y.nothing;
  s;
}
