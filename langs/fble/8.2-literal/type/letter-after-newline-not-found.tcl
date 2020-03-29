fble-test-error 14:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C, Unit@ '
');
  % Enums = @(A: Enum@(A: Unit), B: Enum@(B: Unit), C: Enum@(C: Unit), '
': Enum@('
': Unit));

  # The letter X is not part of Enum@. Insert a newline into the word to make
  # sure we do proper tracking of the error location.
  Enums|'BAC
CAX'; 
}
