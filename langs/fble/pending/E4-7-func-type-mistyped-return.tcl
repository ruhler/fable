fble-test-error 6:27 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The return type is not a type
  @ F = \(Bool x, Bool y; Unit@());
  F;
}
