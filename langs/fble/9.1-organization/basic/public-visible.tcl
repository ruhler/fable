fble-test {
  # A public module X is visible to grandparent, parent, aunt, sibling, and
  # child modules:
  #
  #  /-> P--> X -> C
  # .     \-> S
  #  \-> A

  # Load all modules to force compilation of those.
  { /A%; @(); };
  { /P%; @(); };
  { /P/S%; @(); };
  { /P/X%; @(); };
  { /P/X/C%; @(); };

  @();
} {
  P {
    { /P/X%; @(); };

    @();
  } {
    X {
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
      @();
    }
  }
} {
  A {
    { /P/X%; @(); };
    @();
  }
}
