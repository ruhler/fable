fble-test-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The tail type should match L@, but it doesn't.
  @ L@ = +(*(Unit@ head, Unit@ tail) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
