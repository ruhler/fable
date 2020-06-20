fble-test {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ False = Bool@(false: Unit);

  @ Result@ = +(Unit@ a, *(Unit@ c) b);
  Result@ A = Result@(a: Unit);

  # This is a regression test for a bug we had where this would seg fault
  # because we had mismatching enter/exit calls on the profile stack.
  False.?(true: A, : Result@(b: @(c: Unit)));
}
