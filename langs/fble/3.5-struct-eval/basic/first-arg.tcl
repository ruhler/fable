fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ s = Pair@(Bool@(true: Unit@()), Bool@(false: Unit@()));

  # Test basic struct eval, accessing the first arg of the struct.
  s {
    x.true;
  };
}
