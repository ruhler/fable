fble-test-error 2:11 {
  # A private module X is not visible to its aunt.
  #
  # . -> P -> X*
  #  \-> A

  % _ = /A%;

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
    % _ = /P/X%;
    @();
  }
}
