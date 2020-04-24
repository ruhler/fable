fble-test {
  # Regression test. This was causing a seg fault at one point when switching
  # from reference counting collector to mark sweep collector. Not sure why.
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  <@>@ Monoid@ = /Monoid%.Monoid@;

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ True = Bool@(true: Unit);

  (Bool@){Bool@;} Not = (Bool@ a) { True; };
  (Bool@, Bool@){Bool@;} And = (Bool@ a, Bool@ b) { True; };
  (Bool@, Bool@){Bool@;} Or = (Bool@ a, Bool@ b) { True; };
  (Bool@, Bool@){Bool@;} Eq = (Bool@ a, Bool@ b) { True; };

  Monoid@<Bool@> AndMonoid = Monoid@<Bool@>(And, True);
  @();
} {
  Monoid {
    <@>@ Monoid@ = <@ T@> { *((T@, T@){T@;} op, T@ x); };
    @(Monoid@);
  } {}
}
