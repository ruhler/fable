fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  # We should be able to do sustained put and get on a port without growing
  # memory.
  (Nat@ x) {

    Unit@ ~ get, put;

    (Nat@){ Unit@!; } f = (Nat@ n) {
      ?(n;
          S: {
            Unit@ _ := put(Unit@());
            Unit@ _ := get;
            f(n.S);
          },
          Z: $(Unit@()));
    };
    f(x);
  };
}
