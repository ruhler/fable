fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;

  # Executing f(n) requires O(1) memory.
  (Nat@){ Unit@!; } f = (Nat@ n) {
    $(Unit@());
  };

  f;
}
