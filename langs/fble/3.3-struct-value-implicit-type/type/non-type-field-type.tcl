fble-test-compile-error 9:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # The second field should not have a type as its value given that it is named
  # 'y', but it is given a normal value instead.
  @(x: true, y: Unit@);
}
