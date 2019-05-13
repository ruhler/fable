fble-test-error 12:15 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  Maybe@ y = nothing;

  # The last argument is entirely missing.
  @(x: true,  );
}
