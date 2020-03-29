fble-test {
  # A private module X is visible to parent, sibling, and child modules.
  # The public child module C is visible to parent and sibling.
  #
  # . -> P -> X* -> C
  #       \-> S

  % _ = /P%;
  % _ = /P/S%;

  @();
} {
  P {
    % _ = /P/X%;
    % _ = /P/X/C%;

    @();
  } {
    "X*" {
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
      % _ = /P/X/C%;
      @();
    }
  }
}
