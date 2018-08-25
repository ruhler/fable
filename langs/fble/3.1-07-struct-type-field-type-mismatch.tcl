fble-test-error 9:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # StructA and StructB are different types because a field type differs.
  @ StructA@ = *(Unit@ x, Bool@ y);
  @ StructB@ = *(Unit@ x, Unit@ y);
  StructA@ x = StructB@(Unit@(), Unit@());
  x;
}
