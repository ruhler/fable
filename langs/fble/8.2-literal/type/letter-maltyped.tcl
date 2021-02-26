fble-test-error 16:6 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  % Enums = @(A: Unit, B: Enum@(B: Unit), C: Enum@(C: Unit));

  % L = @(
    '|': (Unit@ x) { x; },
    ',': (Enum@ e, Unit@ x) { x; },
    '': Unit@(),
    '?': Enums
  );

  # All letters in the literal have to have the same type.
  L|BAC; 
}
