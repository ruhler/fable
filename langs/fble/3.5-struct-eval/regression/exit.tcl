fble-test {
  @ Unit@ = *();

  # We had a bug in the past where we failed to generate an exit instruction
  # if a struct eval occured as the last expression in its block.
  @(Unit@) {
    Unit@();
  };
}
