fble-test-error 8:15 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The union types are different because a field type differs.
  @ UnionA@ = +(Unit@ x, Bool@ y);
  @ UnionB@ = +(Unit@ x, Bool@ y, Unit@ z);
  UnionA@ x = UnionB@(x: Unit@());
  x;
}
