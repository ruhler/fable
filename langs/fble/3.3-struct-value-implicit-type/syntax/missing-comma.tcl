fble-test-compile-error 12:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());
  @ Maybe@ = +(Unit@ nothing, Bool@ just);
  Maybe@ nothing = Maybe@(nothing: Unit@());

  Maybe@ y = nothing;

  # The comma is missing bewteen the two args.
  @(x: true  y);
}
