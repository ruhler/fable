fble-test-error 8:4  {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ s = Pair@(Bool@(true: Unit@()), Bool@(false: Unit@()));

  # The struct object has bad syntax.
  ??? {
    x.true;
  };
}
