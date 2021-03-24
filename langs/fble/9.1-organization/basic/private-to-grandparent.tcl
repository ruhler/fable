fble-test-error 6:5 {
  # A private module X is not visible to its grandparent.
  #
  # . -> P -> X*

  { /P/X%; };
} {
  P {
    @();
  } {
    "X*" {
      @();
    }
  }
}
