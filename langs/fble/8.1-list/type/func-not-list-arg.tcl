fble-test-compile-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of L is a function, not a list type.
  @ L@ = (Unit@) { Unit@; };

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
