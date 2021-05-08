fble-test-compile-error 10:8 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  Bool@ x = true;

  # The variable 'y' has not been defined.
  @(x, y);
}
