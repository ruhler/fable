fble-test-error 4:25 {
  # The type for the second field of the union is not a type.
  @ Unit = *();
  @ Bool = +(Unit true, Unit@() false);
  Unit;
}
