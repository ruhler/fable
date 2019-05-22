fble-test-error 7:14 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # A process type and a process input type are two different types.
  Unit@! x = $(Unit@());
  Unit@- y = x;
  x;
}
