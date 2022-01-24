fble-test-compile-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of 'nil' is a function, but it should be *().
  @ L@ = +(*(Unit@ head, L@ tail) cons, (Unit@) { Unit@; } nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
