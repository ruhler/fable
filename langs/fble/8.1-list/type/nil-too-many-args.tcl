fble-test-compile-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of 'nil' has fields, but it should be *().
  @ L@ = +(*(Unit@ head, L@ tail) cons, *(Unit@ x, Unit@ y) nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
