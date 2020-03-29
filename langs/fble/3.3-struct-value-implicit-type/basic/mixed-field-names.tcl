fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  Maybe@ y = nothing;

  # Implicit type struct value with some explicit and some implicit field names.
  @ S@ = *(Bool@ x, Maybe@ y);
  S@ s = @(x: true, y);
  Unit@ _a = s.x.true;
  Unit@ _b = s.y.nothing;
  s;
}
