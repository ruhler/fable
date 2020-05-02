fble-test {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;
  % Nat = /Nat%.Nat;
  % Bit = /Nat%.Bit;

  # Calling tail recursive f should not smash the stack, even for large N.
  (Nat@){ Unit@; } f = (Nat@ n) {
    S@ s = S(n);
    ?(s; Z: Unit@(), S: f(s.S));
  };

  Nat@ n = Nat(Bit|10000000000000000);

  f(n);
}
