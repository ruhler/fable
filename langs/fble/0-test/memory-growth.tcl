fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  [Nat@][Nat@]{ Nat@; } add = [Nat@ n][Nat@ sum] {
    ?(n; S: add[n.S][Nat@(S: sum)], Z: sum);
  };

  # f requires O(N) memory.
  [Nat@]{ Nat@; } f = [Nat@ n] {
    add[n][n];
  };

  f;
}
