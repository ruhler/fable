fble-test-error 16:8 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  % Enums = @(A: Enum@(A: Unit), B: Enum@(B: Unit), C: Enum@(C: Unit));

  % L = @(
    '|': (Unit@ x) { x; },
    ',': (Enum@ e, Unit@ x) { x; },
    '': Unit@(),
    '?': Enums
  );

  # The letter X is not part of Enum@.
  L|BACX; 
}
