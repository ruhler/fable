fble-test-error 8:14 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The union types are different because a field name differs.
  @ UnionA = +(Unit x, Bool y);
  @ UnionB = +(Unit x, Bool z);
  UnionA x = UnionB@(x: Unit@());
  x;
}
