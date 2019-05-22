fble-test-error 7:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The argument types don't match.
  (Unit@+ x) {
    Bool@+ y = x;
    y;
  };
}
