fble-test-error 2:8 {
  # A private module X is not visible to its aunt.
  #
  # . -> P -> X*
  #  \-> A

  { /A%; @(); };

  @();
} {
  P {
    @();
  } {
    "X*" {
      @();
    }
  }
} {
  A {
    { /P/X%; @(); };
    @();
  }
}
