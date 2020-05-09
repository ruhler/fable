fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;

  @ Func@ = (Unit@) { Unit@; };

  (Func@, Nat@){ Func@; } f = (Func@ x, Nat@ n) {
    # The variable x should not be captured by the function g, because
    # x is not referenced in the body of g. That means f should use
    # constant memory, not linear.
    Func@ g = (Unit@ _) { _; };
    S@ s = S(n);
    s.?(Z: g, S: f(g, s.S));
  };

  (Nat@ n) { f((Unit@ x) { x; }, n); };
}
