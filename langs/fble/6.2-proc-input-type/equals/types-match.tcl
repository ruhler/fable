fble-test {
  @ Unit@ = *();

  # The two process input types are equal.
  (Unit@- x) {
    Unit@- y = x;
    y;
  };
}
