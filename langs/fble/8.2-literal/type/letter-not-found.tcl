fble-test-compile-error 12:8 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  @ Enum@ = +(Unit@ A, Unit@ B, Unit@ C);

  @ L@ = +(*(Enum@ head, L@ tail) cons, Unit@ nil);

  (L@) { L@; } f = (L@ l) { l; };

  # The letter X is not part of Enum@.
  f|BACX; 
}
