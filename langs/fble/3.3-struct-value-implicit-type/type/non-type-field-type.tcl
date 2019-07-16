fble-test-error 9:17 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The second field should not have a type as its value given it is named 'y',
  # but it is given a normal value instead.
  @(x: true, y: Unit@);
}
