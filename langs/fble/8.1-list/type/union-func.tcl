fble-test-compile-error 9:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);
  Enum@ enum = Enum@(A: Unit);

  # A union value cannot be used for a literal function.
  enum[Unit, Unit]; 
}
