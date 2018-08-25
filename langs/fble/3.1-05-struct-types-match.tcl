fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # Two struct types with the same fields match.
  @ StructA@ = *(Unit@ x, Bool@ y);
  @ StructB@ = *(Unit@ x, Bool@ y);
  StructA@ x = StructB@(Unit@(), true);
  x;
}
