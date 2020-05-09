fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;

  @ Sum@ = +(Sum@ S, Unit@ Z);

  (Nat@, Sum@){ Sum@; } add = (Nat@ n, Sum@ sum) {
    S@ s = S(n);
    s.?(Z: sum, S: add(s.S, Sum@(S: sum)));
  };

  # Evaluating f requires O(1) memory, but executing the resulting process
  # requires O(N) memory.
  (Nat@){ Sum@!; } f = (Nat@ n) {
    $(add(n, Sum@(Z: Unit@())));
  };

  f;
}
