fble-test-error 4:31 {
  # The fields of a struct type must have different names.
  @ Unit = *();
  @ UnitPair = *(Unit x, Unit x);
  Unit;
}
