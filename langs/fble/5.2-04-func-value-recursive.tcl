fble-test {
  @ Unit@ = *();
  @ Int@ = +(Unit@ 0, Unit@ 1, Unit@ 2, Unit@ 3);

  # There should be no issue with having zeroify be recursively defined.
  \(Int@; Int@) zeroify = \(Int@ x) {
    ?(x; 0: x,
         1: zeroify(Int@(0: Unit@())),
         2: zeroify(Int@(1: Unit@())),
         3: zeroify(Int@(2: Unit@())));
  };
  zeroify(Int@(3: Unit@())).0;
}
