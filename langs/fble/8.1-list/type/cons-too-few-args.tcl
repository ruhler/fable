fble-test-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # There ought to be two few fields in the cons type.
  @ L@ = +(*(Unit@ x) cons, Unit@ nil);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
