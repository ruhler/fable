fble-test-error 7:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # An input type and an output type are two different types.
  (Unit@- x) {
    Unit@+ y = x;
    y;
  };
}
