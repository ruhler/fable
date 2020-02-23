fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  @ Func@ = (Unit@) { Unit@; };

  (Func@, Nat@){ Func@; } f = (Func@ x, Nat@ n) {
    # The variable x is captured by the function g, because x is referenced in
    # the body of g. That means f should use linear memory.
    Func@ g = (Unit@ _) { x(_); };
    ?(n; S: f(g, n.S), Z: g);
  };

  f((Unit@ x) { x; });
}
