fble-test-compile-error 10:3 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ t = Bool@(true: Unit@());
  Bool@ f = Bool@(false: Unit@());

  # The condition isn't a union type
  @ Pair@ = *(Unit@ true, Unit@ false);
  Pair@ s = Pair@(Unit@(), Unit@());
  s.?(true: f, false: t);
}
