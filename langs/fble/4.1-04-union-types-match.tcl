fble-test {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # Two union types with the same fields match.
  @ UnionA@ = +(Unit@ x, Bool@ y);
  @ UnionB@ = +(Unit@ x, Bool@ y);
  UnionA@ x = UnionB@(x: Unit@());
  x;
}
