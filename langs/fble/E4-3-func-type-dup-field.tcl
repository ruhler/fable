fble-test-error 6:24 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The arguments to a function must have different names. 
  @ F = \(Bool x, Bool x; Bool);
  F;
}
