fble-test {
  # A function type that has a two arguments
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  @ F = \(Bool x, Bool y; Bool);
  F;
}
