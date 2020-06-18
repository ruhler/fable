fble-test-error 9:10 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  % Enums = @(A: Unit, B: Enum@(B: Unit), C: Enum@(C: Unit));

  # All letters in the literal have to have the same type.
  Enums|BAC; 
}
