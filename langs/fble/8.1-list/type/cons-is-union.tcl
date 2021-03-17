fble-test-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of 'cons' is a union, but it should be a struct.
  @ L@ = +(+(Unit@ x, Unit@ y) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
