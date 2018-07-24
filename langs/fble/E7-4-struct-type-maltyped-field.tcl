fble-test-error 4:26 {
  # The type for the second field of the struct does not compile.
  @ Unit = *();
  @ UnitPair = *(Unit x, zzz y);
  Unit;
}
