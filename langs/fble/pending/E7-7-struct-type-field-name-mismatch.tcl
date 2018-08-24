fble-test-error 9:15 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);
  Bool true = Bool@(true: Unit@());

  # StructA and StructB are different types because the field name differs.
  @ StructA = *(Unit x, Bool y);
  @ StructB = *(Unit x, Bool z);
  StructA x = StructB@(Unit@(), true);
  x;
}
