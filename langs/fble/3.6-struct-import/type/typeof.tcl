fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  @ Pair@ = *(Bool@ x, Bool@ y);
  Pair@ s = Pair@(Bool@(true: Unit@()), Bool@(false: Unit@()));

  # The type of the struct eval is the type of its body.
  Bool@ t = { s; x; };
  t;
}
