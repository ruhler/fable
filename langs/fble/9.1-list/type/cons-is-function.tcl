fble-test-compile-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of 'cons' is a function, but it should be a struct.
  @ L@ = +((Unit@) { Unit@; } cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
