fble-test-error 7:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The argument types don't match.
  Unit@! x = !(Unit@());
  Bool@! y = x;
  x;
}
