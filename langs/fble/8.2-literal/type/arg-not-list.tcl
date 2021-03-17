fble-test-error 10:3 {
  @ Unit@ = *();
  Unit@ Unit = Unit@();

  # The argument to the function f should be a list type.
  @ L@ = Unit@;

  (L@) { Unit@; } f = (L@ _) { Unit@(); };

  f|'';
}
