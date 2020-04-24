fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;

  # f requires O(1) memory.
  (Nat@){ Unit@; } f = (Nat@ n) {
    Unit@();
  };

  f;
}
