fble-test-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The element type should be a union type, not Unit@.
  @ L@ = +(*(Unit@ head, L@ tail) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ _) { Unit@(); };

  f|'';
}
