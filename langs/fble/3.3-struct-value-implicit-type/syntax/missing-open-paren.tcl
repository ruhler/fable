fble-test-error 12:6 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  Maybe@ y = nothing;

  # The initial open paren is missing.
  @ x: true, y);
}
