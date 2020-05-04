fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;

  @ Func@ = (Unit@) { Unit@; };

  (Func@, Nat@){ Func@; } f = (Func@ x, Nat@ n) {
    # The variable x is captured by the function g, because x is referenced in
    # the body of g. That means f should use linear memory.
    Func@ g = (Unit@ _) { x(_); };
    S@ s = S(n);
    ?(s; Z: g, S: f(g, s.S));
  };

  (Nat@ n) { f((Unit@ x) { x; }, n); };
}
