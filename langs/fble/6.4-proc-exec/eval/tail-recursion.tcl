fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;

  # f should only require O(1) memory, because it is tail recursive.
  (Nat@){ Unit@!; } f = (Nat@ n) {
    S@ s = S(n);
    ?(s;
      Z: $(Unit@()),
      S: { Nat@ sn := $(s.S); f(sn); });
  };

  f;
}
