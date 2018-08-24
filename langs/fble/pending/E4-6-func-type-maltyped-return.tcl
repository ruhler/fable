fble-test-error 6:27 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The return type does not compile
  @ F = \(Bool x, Bool y; zzz);
  F;
}
