fble-test-memory-growth {
  @ Unit@ = *();
  @ Nat@ = /Nat%.Nat@;
  @ S@ = /Nat%.S@;
  % S = /Nat%.S;

  # If we keep putting values onto a link and never take them off, that's
  # memory growth. This test is primarily to double check that the 
  # sustained-put-get test will fail if there is a memory leak.
  (Nat@ x) {

    Unit@ ~ get, put;

    (Nat@){ Unit@!; } f = (Nat@ n) {
      S@ s = S(n);
      ?(s;
          Z: $(Unit@()),
          S: {
            Unit@ _ := put(Unit@());
            f(s.S);
          });
    };
    f(x);
  };
}
