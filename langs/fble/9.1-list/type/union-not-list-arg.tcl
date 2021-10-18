fble-test-compile-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The type of L has too few arguments to be a list type.
  @ L@ = +(Unit@ single);

  (L@) { Unit@; } f = (L@ _) { Unit; };

  f[];
}
