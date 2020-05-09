fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;

  # We should be able to do sustained put and get on a port without growing
  # memory.
  (Nat@ x) {

    Unit@ ~ get, put;

    (Nat@){ Unit@!; } f = (Nat@ n) {
      S@ s = S(n);
      s.?(
          Z: $(Unit@()),
          S: {
            Unit@ _ := put(Unit@());
            Unit@ _ := get;
            f(s.S);
          });
    };
    f(x);
  };
}
