fble-test-error 9:19 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());
  Bool@ false = Bool@(false: Unit@());

  # Verify that arguments are evaluated when the list is evaluated, before the
  # resulting function is applied.
  [Unit@(), false.true];
}
