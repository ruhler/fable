fble-test-error 6:23 {
  @ Unit@ = *();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  # All arguments to the list must have the same type.
  [Enum@(B: Unit@()), Unit@(), Enum@(C: Unit@())];
}
