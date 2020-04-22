fble-test-memory-constant {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  # We should be able to do sustained put and get on a port without growing
  # memory. Even if the object we are putting on the link is a cycle that
  # references the link.
  # This is a regression test for a bug where we failed to properly break
  # nested cycles.
  (Nat@ x) {

    Unit@! ~ get, put;

    (Nat@){ Unit@!; } f = (Nat@ n) {
      ?(n;
          S: {
            Unit@! a = { Unit@! _ := get; b; }, 
            Unit@! b = { Unit@! _ := get; a; };

            Unit@ _ := put(a);
            Unit@! _ := get;

            f(n.S);
          },
          Z: $(Unit@()));
    };
    f(x);
  };
}
