fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = +(Nat@ S, Unit@ Z);

  # If we keep putting values onto a link and never take them off, that's
  # memory growth. This test is primarily to double check that the 
  # sustained-put-get test will fail if there is a memory leak.
  (Nat@ x) {

    Unit@ ~ get, put;

    (Nat@){ Unit@!; } f = (Nat@ n) {
      ?(n;
          S: {
            Unit@ _ := put(Unit@());
            f(n.S);
          },
          Z: $(Unit@()));
    };
    f(x);
  };
}
