fble-test {
  # A function type that has a single argument.
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  @ F = \(Bool x; Bool);
  F;
}
