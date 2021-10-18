fble-test-compile-error 15:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C, Unit@ '
');

  @ L@ = +(*(Enum@ head, L@ tail) cons, Unit@ nil);

  (L@) { L@; } f = (L@ l) { l; };

  # The letter X is not part of Enum@. Insert a newline into the word to make
  # sure we do proper tracking of the error location.
  f|'BAC
CAX'; 
}
