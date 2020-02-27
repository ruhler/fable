fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  @ Func@ = (Unit@) { Unit@; };

  (Func@, Nat@){ Func@; } f = (Func@ x, Nat@ n) {
    # The variable x is not captured by the function g, because x is
    # only referenced in the body of g in a typeof expression. That means f
    # should use linear memory.
    Func@ g = (Unit@ _) {
      @(tx@: @<x>, u: Unit@()).u;
    };
    ?(n; S: f(g, n.S), Z: g);
  };

  f((Unit@ x) { x; });
}
