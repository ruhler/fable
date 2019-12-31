fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  (Nat@, Nat@){ Nat@; } add = (Nat@ n, Nat@ sum) {
    ?(n; S: add(n.S, Nat@(S: sum)), Z: sum);
  };

  # Evaluating f requires O(1) memory, but executing the resulting process
  # requires O(N) memory.
  (Nat@){ Nat@!; } f = (Nat@ n) {
    # TODO: If we change $(...) to only evaluate when the process is executed,
    # we don't need this Unit@ _ := ... hack.
    Unit@ _ := $(Unit@());
    $(add(n, n));
  };

  f;
}
