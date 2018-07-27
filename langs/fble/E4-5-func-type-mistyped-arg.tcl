fble-test-error 6:19 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The type for the second arg of the function is not a type.
  @ F = \(Bool x, Unit@() y; Bool);
  F;
}
