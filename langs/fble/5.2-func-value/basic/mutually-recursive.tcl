fble-test {
  @ Unit@ = *();
  @ Int@ = +(Unit@ 0, Unit@ 1, Unit@ 2);

  (Int@) { Int@; } f = (Int@ x) {
    x.?(0: x, 1: g(Int@(0: Unit@())), 2: g(Int@(1: Unit@())));
  },
  (Int@) { Int@; } g = (Int@ x) {
    x.?(0: x, 1: f(Int@(0: Unit@())), 2: f(Int@(1: Unit@())));
  };

  f(Int@(2: Unit@())).0;
}
