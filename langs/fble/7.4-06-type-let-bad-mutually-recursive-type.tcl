fble-test-error 3:115 {
  # Regression test for a bug we had in the past where this would leak types.
  { @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(C@ a); A@; }({ @ A@ = *(B@ b), @ B@ = *(C@ c), @ C@ = *(A@ a); C@; }(*()()));
}
