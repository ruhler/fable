fble-test-error 9:9 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  @(Enums: @(A: Enum@(A: Unit), B: Enum@(B: Unit), C: Enum@(C: Unit)));

  # Empty string literals are not allowed
  Enums|''; 
}
