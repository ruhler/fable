fble-test-error 7:8 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The open paren is missing.
  Bool@) { true; };
}
