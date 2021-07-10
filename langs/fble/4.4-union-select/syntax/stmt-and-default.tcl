fble-test-compile-error 10:28 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit);

  # You're not allowed to use the statement syntax if you also provide an
  # explicit default branch.
  t.?(true: Unit, : Unit); Unit;
}
