fble-test-error 4:26 {
  # The type for the second field of the struct is not a type.
  @ Unit = *();
  @ UnitPair = *(Unit x, Unit@() y);
  Unit;
}
