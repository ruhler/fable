fble-test-error 6:5 {
  # A public child of a private module is not visible to grandparent.
  #
  # . -> P -> X* -> C

  { /P/X/C%; };
} {
  P {
    @();
  } {
    "X*" {
      @();
    } {
      C {
        @();
      }
    }
  }
}
