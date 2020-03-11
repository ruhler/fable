fble-test {
  # A private module X is visible to parent, sibling, and child modules.
  # The public child module C is visible to parent and sibling.
  #
  # . -> P -> X* -> C
  #       \-> S

  { /P%; @(); };
  { /P/S%; @(); };

  @();
} {
  P {
    { /P/X%; @(); };
    { /P/X/C%; @(); };

    @();
  } {
    "X*" {
      @();
    } {
      C {
        { /P/X%; @(); };
        @();
      }
    }
  } {
    S {
      { /P/X%; @(); };
      { /P/X/C%; @(); };
      @();
    }
  }
}
