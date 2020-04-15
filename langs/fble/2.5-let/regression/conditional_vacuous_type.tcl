fble-test-error 7:5 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Regression test for a funny way to define a vacuous type.
  @ T@ = ?(true; true: T@, false: T@);

  T@();
}
