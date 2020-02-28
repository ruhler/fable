fble-test {
  @ Unit@ = *();
  Unit@ x = Unit@();

  # Regression test for a bug where we mixed up the number of local slots with
  # the number of local variables. The result was a seg fault iterating over
  # the total number of slots when we really wanted to iterate over the total
  # number of variables in scope.
  (Unit@ _) {
    Unit@ a = {
      Unit@ b = Unit@();
      Unit@ c = b;
      c;
    };
    x;
  };
}
