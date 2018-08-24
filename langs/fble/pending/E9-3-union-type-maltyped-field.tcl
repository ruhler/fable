fble-test-error 4:25 {
  # The type for the second field of the union does not compile.
  @ Unit = *();
  @ Bool = +(Unit true, zzz false);
  Unit;
}
