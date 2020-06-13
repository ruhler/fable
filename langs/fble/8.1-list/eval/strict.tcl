fble-test-error 8:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ false = Bool@(false: Unit@());

  # Verify that arguments are evaluated when the list is evaluated, before the
  # resulting function is applied.
  [Unit@(), false.true];
}
