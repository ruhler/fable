fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  # f requires O(1) memory.
  (Nat@){ Unit@; } f = (Nat@ n) {
    Unit@();
  };

  f;
}
