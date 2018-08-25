fble-test-error 10:16 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);
  Bool@ true = Bool@(true: Unit@());

  # StructA and StructB are different types because the number of fields is
  # different.
  @ StructA@ = *(Unit@ x, Bool@ y);
  @ StructB@ = *(Unit@ x, Bool@ y, Unit@ z);
  StructA@ x = StructB@(Unit@(), true, Unit@());
  x;
}
