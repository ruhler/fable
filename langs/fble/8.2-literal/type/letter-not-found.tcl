fble-test-error 9:12 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  % Enums = @(A: Enum@(A: Unit), B: Enum@(B: Unit), C: Enum@(C: Unit));

  # The letter X is not part of Enum@.
  Enums|BACX; 
}
