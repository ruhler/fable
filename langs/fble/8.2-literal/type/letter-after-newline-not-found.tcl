fble-test-error 9:3 {
  @ Unit@ = *();
  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C, Unit@ '
');

  # The letter X is not part of Enum@. Insert a newline into the word to make
  # sure we do proper tracking of the error location.
  Enum@|'BAC
CAX'; 
}
