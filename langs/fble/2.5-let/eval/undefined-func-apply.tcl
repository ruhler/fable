fble-test-runtime-error 8:21 {
  @ Unit@ = *();
  @ Bool@ = +(Unit@ true, Unit@ false);

  # The value of 'f' is undefined in the let bindings. This will produce
  # undefined behavior when applying the undefined function f.
  (Bool@) { Bool@; } f = {
     Bool@ result = f(Bool@(true: Unit@()));
     (Bool@ b) { result; };
  };
  Unit@();
}
