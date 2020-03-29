fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  Bool@ x = true;
  Maybe@ y = nothing;

  # Implicit type struct value with implicit field names.
  @ S@ = *(Bool@ x, Maybe@ y);
  S@ s = @(x, y);
  Unit@ _a = s.x.true;
  Unit@ _b = s.y.nothing;
  s;
}
