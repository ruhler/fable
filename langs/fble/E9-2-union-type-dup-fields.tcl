fble-test-error 4:30 {
  # The fields of the union must have different names.
  @ Unit = *();
  @ Bool = +(Unit true, Unit true);
  Unit;
}
