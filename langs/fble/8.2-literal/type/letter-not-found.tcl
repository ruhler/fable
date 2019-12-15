fble-test-error 6:12 {
  @ Unit@ = *();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  # The letter X is not part of Enum@.
  Enum@|BACX; 
}
