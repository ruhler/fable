fble-test-error 8:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The comma is missing between first and second choice.
  t.?(true: f  false: t);
}
