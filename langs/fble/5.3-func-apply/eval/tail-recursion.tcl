fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  # f should only require O(1) memory, because it is tail recursive.
  (Nat@){ Unit@; } f = (Nat@ n) {
    ?(n; S: f(n.S), Z: Unit@());
  };

  f;
}
