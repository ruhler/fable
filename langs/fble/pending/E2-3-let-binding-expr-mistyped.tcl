fble-test-error 6:12 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The binding expression has the wrong type.
  Bool x = Unit@();
  x;
}
