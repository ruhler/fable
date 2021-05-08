fble-test-runtime-error 7:13 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The value of 'b' is undefined in the let bindings. This will produce
  # undefined behavior when doing select on the undefined value b.
  Bool@ b = b.?(true: Bool@(false: Unit@()), false: Bool@(true: Unit@()));
  Unit@();
}
