fble-test-error 7:10 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # The close paren is missing.
  (Bool@ { true; };
}
