fble-test-compile-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of L is a struct type, not a union as expected for a list.
  @ L@ = *(*(Unit@ x, L@ l) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
