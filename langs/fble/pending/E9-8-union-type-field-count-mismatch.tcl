fble-test-error 8:14 {
  @ Unit = *();
  @ Bool = +(Unit true, Unit false);

  # The union types are different because the number of fields differs.
  @ UnionA = +(Unit x, Bool y);
  @ UnionB = +(Unit x, Bool y, Unit z);
  UnionA x = UnionB@(x: Unit@());
  x;
}
