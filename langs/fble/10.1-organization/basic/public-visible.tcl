fble-test {
  # A public module X is visible to grandparent, parent, aunt, sibling, and
  # child modules:
  #
  #  /-> P--> X -> C
  # .     \-> S
  #  \-> A

  # Load all modules to force compilation of those.
  % _ = /A%;
  % _ = /P%;
  % _ = /P/S%;
  % _ = /P/X%;
  % _ = /P/X/C%;

  @();
} {
  P {
    % _ = /P/X%;

    @();
  } {
    X {
      @();
    } {
      C {
        % _ = /P/X%;
        @();
      }
    }
  } {
    S {
      % _ = /P/X%;
      @();
    }
  }
} {
  A {
    % _ = /P/X%;
    @();
  }
}
