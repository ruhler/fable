fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  @ Func@ = (Unit@) { Unit@; };

  (Func@, Nat@){ Func@; } f = (Func@ x, Nat@ n) {
    # The variable x should not be captured by the function g, because
    # x is not referenced in the body of g. That means f should use
    # constant memory, not linear.
    Func@ g = (Unit@ _) { _; };
    ?(n; S: f(g, n.S), Z: g);
  };

  f((Unit@ x) { x; });
}
