fble-test-error 9:18 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The second field should have a type as its value given it is named 'y@',
  # but it is given a normal value instead.
  @(x: true, y@: true);
}
