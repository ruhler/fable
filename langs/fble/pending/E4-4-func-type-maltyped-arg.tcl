fble-test-error 6:19 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The type for the second arg of the function does not compile.
  @ F = \(Bool x, zzz y; Bool);
  F;
}
