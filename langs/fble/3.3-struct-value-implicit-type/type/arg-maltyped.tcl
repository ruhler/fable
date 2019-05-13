fble-test-error 8:17 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The second argument is maltyped.
  @(x: true, y: zzz);
}
